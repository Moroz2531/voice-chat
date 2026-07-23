#include <mainserver.hpp>

using namespace messenger;

namespace {
constexpr uint32_t EPOLL_DEFAULT_PARAMS =
    EPOLLET | EPOLLONESHOT | EPOLLIN | EPOLLERR | EPOLLHUP;
constexpr auto EPOLL_MAX_EVENTS = 50;
constexpr auto EPOLL_TIMEOUT_MS = 20;
};  // namespace

MainServer::MainServer(uint64_t id) : BaseServer(id) {
  jt_ = std::jthread([&](std::stop_token stok) {
    try {
      set_run();
      std::jthread jt{[&](std::stop_token stok) {
        try {
          handler_accept(stok);
        } catch (const std::exception& e) {
          set_warn();
        }
      }};
      handler_users(stok);
    } catch (const std::exception& e) {
      set_warn();
    }
  });
}

bool MainServer::local_ip(in_addr_t& addr) const noexcept {
  return acc_.get()[rand() % acc_.get().size()]->local_ip(addr);
}

bool MainServer::local_port(in_port_t& port) const noexcept {
  return acc_.get()[rand() % acc_.get().size()]->local_port(port);
}

MainServer::Acceptor::Acceptor(size_t cnt)
    : accpt_{
          cnt,
          std::make_unique<Socket>(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0)} {
  auto sin{createSockaddrIn(AF_INET, 0, INADDR_ANY)};
  std::for_each(accpt_.begin(), accpt_.end(), [&](auto sfd) {
    sfd->bind(reinterpret_cast<sockaddr*>(&sin), sizeof(sin));
    sfd->listen();
    ep_.insert(*sfd,
               epoll_event{.events = EPOLL_DEFAULT_PARAMS, .data.ptr = &sfd});
  });
}

bool MainServer::Acceptor::insert(Socket&& sfd) {
  auto& tmp = sfd;
  auto retval = gsts_.insert(std::make_pair<int, Guest>(
      sfd, Guest{std::make_unique<Socket>(std::move(sfd))}));
  if (retval) {
    ep_.insert(tmp,
               epoll_event{.events = EPOLL_DEFAULT_PARAMS, .data.fd = tmp});
    return true;
  }
  return false;
}

void MainServer::handler_accept(std::stop_token stok) {
  auto handle_accept = [&](const epoll_event& ev) {
    auto&& sfd = *static_cast<std::unique_ptr<Socket>*>(ev.data.ptr);
    acc_.insert(std::move(sfd->accept4(nullptr, nullptr, SOCK_NONBLOCK)));
  };

  auto foreach_events = [&](std::span<const epoll_event> evs) {
    for (auto&& i : evs) {
      if (i.events & EPOLLIN) {
      }
      if (i.events & (EPOLLERR | EPOLLHUP)) {
      }
    }
  };

  epoll_event evs[EPOLL_MAX_EVENTS];
  while (!stok.stop_requested()) {
    auto c = acc_.wait(std::span{evs, EPOLL_MAX_EVENTS}, EPOLL_TIMEOUT_MS);
    try {
      foreach_events(std::span{evs, c});
    } catch (const std::exception& e) {
      // записать ошибку в файл
    }
  }
}

void MainServer::handler_users(std::stop_token stok) {
  auto loop_epoll = [&] {
    epoll_event evs[EPOLL_MAX_EVENTS];
    while (!stok.stop_requested()) {
      // auto cev = ep_.wait(evs, EPOLL_MAX_EVENTS, EPOLL_TIMEOUT_MS);
      try {
        // foreach_events(std::span{evs, cev});
      } catch (const std::exception& e) {
      }
    }
  };
  loop_epoll();
}
