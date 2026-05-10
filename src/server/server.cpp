#include <format>
#include <iostream>
#include <stdexcept>
#include <syncstream>
#include <utility>
#include <vector>

#include "server.hpp"

using namespace server;

namespace {
epoll_event eventCreate(uint32_t events, int fd) {
  epoll_event ev;
  ev.events = events;
  ev.data.fd = fd;
  return ev;
}

template <bool remote = false>
std::string getIPv4(const containers::Socket& sfd) noexcept {
  char buf[INET_ADDRSTRLEN];
  in_addr addr;
  addr.s_addr = sfd.ip<remote>();

  if (inet_ntop(AF_INET, &addr, buf, sizeof(buf)) == NULL) {
    return std::string("printIPv4: error inet_ntop");
  }
  return std::string(std::format("{}/{}", buf, ntohs(sfd.port<remote>())));
}
}  // namespace

Server::Server(size_t id) : id_{id} {}

void Server::run() {
  std::lock_guard lock{mutJt_};
  if (jt_.joinable())
    throw std::runtime_error("the server is already running!");
  jt_ = std::jthread([this](std::stop_token stok) { runLoop(stok); });
}

void Server::stop() {
  std::lock_guard lock{mutJt_};
  if (jt_.joinable()) {
    jt_.request_stop();
    jt_.join();
  }
}

bool Server::joinable() const noexcept {
  return jt_.joinable();
}

void Server::insert(const containers::Socket& sfd) {
  std::unique_lock lock{mut_};
  sfds_[sfd] = std::make_shared<DataUser>(sfd);
  lock.unlock();
  epoll_event ev =
      eventCreate(EPOLLIN | EPOLLOUT | EPOLLERR | EPOLLHUP | EPOLLRDHUP, sfd);
  ep_.insert(sfd, ev);
}

void Server::erase(const containers::Socket& sfd) {
  if (contains(sfd)) {
    ep_.erase(sfd);
    std::lock_guard lock{mut_};
    sfds_.erase(sfd);
  }
}

size_t Server::size() const {
  std::lock_guard lock{mut_};
  return sfds_.size();
}

bool Server::empty() const {
  std::lock_guard lock{mut_};
  return sfds_.empty();
}

bool Server::contains(const containers::Socket& sfd) const {
  std::lock_guard lock{mut_};
  return sfds_.contains(sfd);
}

void Server::runLoop(std::stop_token stok) {
  try {
    epoll_event evs[Options::MAX_EVENTS];
    containers::Socket sfd;
    std::string data;
    std::shared_ptr<DataUser> du;

    containers::Parse p;
    ParseInfo pinfo{data, du};

    data.reserve(DATA_BYTES_MAX_LEN);
    fillParse(p, pinfo);

    auto handleInput = [&] {
      data = sfd.recv();
      std::cout << std::format("server get data length {} from {}\n",
                               data.length(), getIPv4<true>(sfd));
      if (data.length() >= 2) {
        uint16_t op;
        std::memcpy(&op, data.data(), 2);
        std::osyncstream(std::cout) << "server (loop): get opcode " << op
                                    << " from " << getIPv4<true>(sfd) << '\n';
        p.execute(op);
      } else if (data.empty()) {
        std::osyncstream(std::cout) << "server (loop): get empty data from "
                                    << getIPv4<true>(sfd) << '\n';
        return -1;
      } else
        throw std::out_of_range(
            "server (loop): receive incorrect message from client (-1)");
      return 0;
    };

    auto handleOutput = [&] {
      if (*du != chs_.size()) {
        try {
          std::vector<char> ids;
          ids.reserve(2 + chs_.size() * sizeof(uint64_t));
          ids.push_back(10);
          ids.push_back(0);
          for (auto& [first, second] : chs_) {
            uint64_t index = *second;
            char* bytes = reinterpret_cast<char*>(&index);
            ids.insert(ids.end(), bytes, bytes + sizeof(uint64_t));
          }
          std::cout << std::format(
              "server (loop): send channels ({} bytes) to {}\n", ids.size(),
              getIPv4<true>(sfd));
          if (sfd.send(ids.data(), ids.size(), 0) == -1)
            return -1;
          *du = chs_.size();
        } catch (const std::runtime_error& re) {
          throw;
        } catch (const std::exception& e) {
          std::osyncstream(std::cerr) << e.what() << '\n';
        }
      }
      if (du->procReq) {
        in_port_t port =
            static_cast<VoiceChannel*>(&*chs_[du->channelIndex])->port();
        char* bytes = reinterpret_cast<char*>(&port);
        char data[4] = {11, 0, bytes[0], bytes[1]};
        std::cout << std::format(
            "server (loop): send port from channels ({} bytes) to {}\n",
            sizeof(data), getIPv4<true>(sfd));
        if (sfd.send(data, sizeof(data), 0) == -1)
          return -1;
        du->procReq = false;
      }
      return 0;
    };

    while (!stok.stop_requested()) {
      int c = ep_.wait(evs, Options::MAX_EVENTS, Options::TIMEOUT_MS);
      for (int i = 0; i < c; ++i) {
        std::unique_lock<std::mutex> lock{mut_};
        auto it = sfds_.find(evs[i].data.fd);
        if (it == sfds_.end())
          continue;
        du = it->second;
        lock.unlock();
        sfd = static_cast<containers::Socket>(*du);
        try {
          if (evs[i].events & (EPOLLIN | EPOLLRDHUP)) {
            if (handleInput()) {
              erase(sfd);
              continue;
            }
          }
          if (evs[i].events & EPOLLOUT) {
            if (handleOutput()) {
              erase(sfd);
              continue;
            }
          }
          if (evs[i].events & (EPOLLERR | EPOLLHUP | EPOLLRDHUP)) {
            erase(sfd);
            continue;
          }
        } catch (const std::out_of_range& oor) {
          std::osyncstream(std::cerr) << oor.what() << '\n';
        } catch (const std::runtime_error& re) {
          erase(sfd);
          std::osyncstream(std::cerr) << re.what() << '\n';
        } catch (const std::exception& e) {
          std::osyncstream(std::cout) << e.what() << '\n';
        }
      }
    }
  } catch (const std::exception& e) {
    std::osyncstream(std::cout) << e.what() << '\n';
  }
}

void Server::fillParse(containers::Parse& p, ParseInfo& pinfo) {
  p.insert(5, [&] {
    if (pinfo.data.length() != 13)
      throw std::out_of_range(
          "server (loop): receive incorrect message from client (5)");
    uint64_t chId;
    in_addr_t ip = pinfo.du->sfd.ip<true>();
    in_port_t port;
    char act;
    std::memcpy(&chId, pinfo.data.data() + 2, 8);
    std::memcpy(&port, pinfo.data.data() + 10, 2);
    std::memcpy(&act, pinfo.data.data() + 12, 1);
    auto ch = static_cast<VoiceChannel*>(&*chs_[chId]);
    if (act) {
      if (ch->contains(ip, port))
        return;
      ch->insert(ip, port);
      if (!ch->joinable())
        ch->run();
      pinfo.du->channelIndex = chId;
      pinfo.du->procReq = true;
      pinfo.du->connectChannel = true;
    } else {
      ch->erase(ip, port);
      if (ch->empty())
        ch->stop();
      pinfo.du->connectChannel = false;
    }
  });
  p.insert(7, [&] {
    chs_.insert(std::pair<size_t, std::unique_ptr<Channel>>{
        chIdx_, std::make_unique<VoiceChannel>(chIdx_)});
    std::osyncstream(std::cout) << "server: add new channel " << chIdx_ << '\n';
    ++chIdx_;
  });
  p.insert(8, [&] {
    if (pinfo.data.length() != 10)
      throw std::out_of_range(
          "server (loop): receive incorrect message from client (8)");
    uint64_t chId;
    std::memcpy(&chId, pinfo.data.data() + 2, 8);
    auto it = chs_.find(chId);
    static_cast<VoiceChannel*>(&*it->second)->clear();
    chs_.erase(it);
    std::osyncstream(std::cout) << "server: delete channel " << chId << '\n';
  });
}
