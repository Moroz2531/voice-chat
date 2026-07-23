#include <boost/container/static_vector.hpp>
#include <boost/lockfree/queue.hpp>
#include <iostream>
#include <queue>
#include <span>
#include <stdexcept>
#include <tuple>
#include <utility>

#include <general/parse.hpp>
#include <lib/epoll.hpp>
#include <server.hpp>

using namespace messenger;

namespace {
constexpr uint32_t DEFAULT_PARAMS_EPOLL =
    EPOLLET | EPOLLIN | EPOLLERR | EPOLLHUP | EPOLLONESHOT;
constexpr uint32_t PARAMS_FOR_SEND_EPOLL = EPOLLOUT | EPOLLERR | EPOLLHUP;
}  // namespace

bool Server::insert(const ISocket& sfd, std::unique_ptr<IUser> u) {
  ServerUsersHashMap::accessor acc;
  conUsers_.insert(acc, sfd);
  epoll_event ev{.events = DEFAULT_PARAMS_EPOLL, .data.ptr = &u};
  ep_.insert(sfd, ev);
  acc->second = std::move(u);
  return true;
}

bool Server::erase(const ISocket& sfd, std::unique_ptr<IUser>& res) noexcept {
  std::unique_ptr<IUser> temp;
  ServerUsersHashMap::accessor acc;
  if (!conUsers_.find(acc, sfd))
    return false;
  ep_.erase(sfd);
  res = std::move(acc->second);
  conUsers_.erase(sfd);
  return true;
}

StatusServer Server::get() const {
  return StatusServer{*this, en_};
}

namespace {
class TaskPool {
  struct Task {
    int sfd;
    std::tuple<> args;

    template <typename... Args>
    Task(int sfd_, Args&&... args_)
        : sfd(sfd_), args(std::forward<Args>(args_)...) {}

    template <typename Handler>
    void apply(Handler&& handler) {
      std::apply(std::forward<Handler>(handler), sfd, args);
    }
  };

  using queueTask =
      boost::lockfree::queue<Task, boost::lockfree::capacity<128>>;

 public:
  TaskPool() = default;

  template <typename... Args>
  void add(int sfd, Args&&... args) {
    Task task(sfd, std::forward<Args>(args)...);
    ts_.push(std::move(task));
  }

  template <typename Handler>
  void process_one(Handler&& handler) {
    Task task;
    if (ts_.pop(task)) {
      task.apply(std::forward<Handler>(handler));
    }
  }

 private:
  queueTask ts_;
};

void connDisconnChannelVoice(int sfd, std::span<char> data, TaskPool& tp) {
#pragma pack(push, 1)
  struct NetworkPacket {
    uint16_t opcode;
    uint64_t chid;
    uint16_t port;
    char action;
  };
#pragma pack(pop, 1)
  if (data.size() != sizeof(NetworkPacket))
    return;
  NetworkPacket pack;
  std::memcpy(&pack, data.data(), sizeof(NetworkPacket));
  tp.add(sfd, pack.opcode, pack.chid, pack.port, pack.action);
}

void createChannelVoice(int sfd, std::span<char> data, TaskPool& tp) {
  if (data.size() <= sizeof(uint16_t) || data.size() % 2 != 0)
    return;
  uint16_t opcode;
  std::u16string name;
  std::memcpy(&opcode, data.data(), sizeof(uint16_t));
  name.reserve(data.size() - sizeof(uint16_t));
  name.resize(data.size() - sizeof(uint16_t));
  std::memcpy(name.data(), data.data() + sizeof(uint16_t),
              data.size() - sizeof(uint16_t));
  tp.add(sfd, opcode, std::move(name));
}

void eraseChannelVoice(int sfd, std::span<char> data, TaskPool& tp) {
#pragma pack(push, 1)
  struct NetworkPacket {
    uint16_t opcode;
    uint64_t chid;
  };
#pragma pack(pop, 1)
  if (data.size() != sizeof(NetworkPacket))
    return;
  NetworkPacket pack;
  std::memcpy(&pack, data.data(), sizeof(NetworkPacket));
  tp.add(sfd, pack.opcode, pack.chid);
}

using ParseMap =
    std::unordered_map<uint16_t,
                       std::function<void(int, std::span<char>, TaskPool&)>>;

void handlerEpoll(std::stop_token stok,
                  const Epoll& ep,
                  ServerLoopSettings& sett,
                  TaskPool& tp) {
  try {
    ParseMap hand;
    hand[5] = connDisconnChannelVoice;
    hand[7] = createChannelVoice;
    hand[8] = eraseChannelVoice;

    auto parse = [&](epoll_event ev) {
      auto&& u = *static_cast<std::unique_ptr<IUser>*>(ev.data.ptr);
      try {
        if (ev.events & EPOLLIN) {
          boost::container::static_vector<char, 1400> data;
          size_t size;
          while (
              (size = u->recv(std::span{data.data(), data.max_size()}, '\n'))) {
            if (size < sizeof(uint16_t))
              continue;
            uint16_t opcode;
            std::memcpy(&opcode, data.data(), sizeof(uint16_t));
            hand[opcode](u->get(), std::span{data.data(), size}, tp);
          }
        }
        if (ev.events & (EPOLLERR | EPOLLHUP)) {
          tp.add(u->get(), 500);
          return;
        }
        epoll_event repeat = {.events = DEFAULT_PARAMS_EPOLL,
                              .data.ptr = ev.data.ptr};
        ep.change(u->get(), repeat);
      } catch (...) {
        tp.add(u->get());
      }
    };
    epoll_event evs[sett.maxEvents];
    while (!stok.stop_requested()) {
      auto c = ep.wait(evs, sett.maxEvents, sett.timeoutMS);
      for (auto i = 0; i < c; ++i)
        parse(evs[i]);
    }
  } catch (...) {
  }
}
}  // namespace

void Server::exec(std::stop_token stok) {
  try {
    ThreadPool thp;
    TaskPool tskp;
    Epoll ep;

    auto epollSend = [&](int sfd, epoll_event newEv, std::span<const char> data,
                         std::unique_ptr<IUser>& usr) {
      constexpr int maxEvs = 5;
      epoll_event evs[maxEvs];
      ep.insert(sfd, newEv);
      while (true) {
        auto c = ep.wait(evs, maxEvs, 1), i = 0;
        for (; i < c; ++i) {
          if (evs[i].data.fd != sfd)
            continue;
          usr->send(data);
          ep.erase(sfd);
          break;
        }
        if (i < c)
          break;
      }
    };

    auto commVoiceChannel = [&](uint64_t chid, int usfd, uint32_t addr,
                                uint16_t port, char act) {
      ServerChannelsHashMap::accessor acc;
      ServerUsersHashMap::accessor uacc;
      if (!chs_.find(acc, chid) || !conUsers_.find(uacc, usfd))
        return;
      auto&& vch = *static_cast<VoiceChannel*>(acc->second.get());
      if (!vch.enable()) {
        auto&& st = vch.get_tid();
        if (st.has_value())
          thp.erase(*st);
        vch.set_tid(thp.add([&](std::stop_token stok) { vch.exec(stok); }));
      }
      in_port_t port;
      while (vch.port(port))
        std::this_thread::yield();
      epollSend(usfd,
                epoll_event{.events = PARAMS_FOR_SEND_EPOLL, .data.fd = usfd},
                std::span{reinterpret_cast<char*>(&port), sizeof(in_port_t)},
                uacc->second);
    };

    auto createVoiceChannel = [&](std::u16string_view name) {};

    auto deleteVoiceChannel = [&](uint64_t chid) {};

    auto handler = [&](int sfd, const auto&... args) {
      auto argsTuple = std::forward_as_tuple(args...);
      switch (std::get<0>(argsTuple)) {
        case 5:
          commVoiceChannel(std::get<1>(argsTuple), sfd, std::get<2>(argsTuple),
                           std::get<3>(argsTuple));
          break;
        case 7:
          createVoiceChannel(std::get<1>(argsTuple));
          break;
        case 8:
          deleteVoiceChannel(std::get<1>(argsTuple));
          break;
        default:
          conUsers_.erase(sfd);
      };
    };

    auto handlerTask = [&](std::stop_token stok, TaskPool& tp) {
      while (!stok.stop_requested()) {
        tp.process_one(handler);
      }
    };

    thp.add(handlerEpoll, std::cref(ep_), std::ref(*this), std::ref(tskp));
    en_.exchange(true);
    handlerTask(stok, tskp);
  } catch (...) {
  }
  en_.exchange(false);
}
