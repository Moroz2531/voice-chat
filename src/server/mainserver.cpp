#include <format>
#include <iostream>
#include <syncstream>

#include "mainserver.hpp"

using namespace server;

MainServer::MainServer() {}

void MainServer::run() {
  if (jt_.joinable())
    throw std::runtime_error("the main server is already running!");
  sfdAccpt_.create(AF_INET, SOCK_STREAM, 0);
  sockaddr_in sin;
  std::memset(&sin, 0, sizeof(sockaddr_in));
  sin.sin_family = AF_INET;
  sin.sin_addr.s_addr = INADDR_ANY;
  sin.sin_port = 0;
  sfdAccpt_.bind(reinterpret_cast<sockaddr*>(&sin), sizeof(sockaddr_in));
  sfdAccpt_.listen();
  jt_ = std::jthread([this](std::stop_token stok) { runLoop(stok); });
}

void MainServer::stop() {
  if (jt_.joinable()) {
    sv_.stop();
    jt_.request_stop();
    jt_.join();
    sfdAccpt_.close();
  }
}

bool MainServer::joinable() const noexcept {
  return jt_.joinable();
}

in_addr_t MainServer::ip() const {
  return sfdAccpt_.ip();
}

in_port_t MainServer::port() const {
  return sfdAccpt_.port();
}

void MainServer::runLoop(std::stop_token stok) {
  char buf[INET_ADDRSTRLEN];
  struct in_addr addr;

  containers::Epoll ep;
  epoll_event ev;
  ev.events = EPOLLIN;
  ep.insert(sfdAccpt_, ev);
  try {
    while (!stok.stop_requested()) {
      if (ep.wait(&ev, 1, 5)) {
        auto sfd = sfdAccpt_.accept4(nullptr, nullptr, SOCK_NONBLOCK);
        sv_.insert(sfd);
        if (sv_.size() == 1)
          sv_.run();
        addr.s_addr = sfd.ip<true>();
        inet_ntop(AF_INET, &addr, buf, sizeof(buf));
        std::cout << std::format("Client connected: {}/{}\n", buf,
                                 sfd.port<true>());
      }
      if (sv_.empty())
        sv_.stop();
    }
  } catch (const std::exception& e) {
    std::osyncstream(std::cerr) << e.what() << '\n';
  }
}
