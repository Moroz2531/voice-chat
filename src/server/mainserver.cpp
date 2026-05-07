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
  jt_ = std::jthread([this](std::stop_token stok) { runLoop(stok); });
}

void MainServer::stop() {
  if (jt_.joinable()) {
    sfdAccpt_.close();
    jt_.request_stop();
    jt_.join();
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
  try {
    while (!stok.stop_requested()) {
      sv.insert(sfdAccpt_.accept());
    }
  } catch (const std::exception& e) {
    std::osyncstream(std::cerr) << e.what() << '\n';
  }
}
