#include <cstring>
#include <iostream>
#include <stdexcept>
#include <syncstream>

#include "client.hpp"

using namespace client;
using namespace std::chrono_literals;

Client::Client() {
  struct __attribute__((packed)) Data {
    uint16_t op{};
    uint64_t cid{};
    uint16_t port{};
    uint8_t act{};
  };

  p_.insert(5, [&] {
    Data data{5, vstream_, vstream_.port(), 0xFF};
    sfd_.send(reinterpret_cast<char*>(&data), sizeof(data), 0);
  });

  p_.insert(6, [&] {
    Data data{5, vstream_, vstream_.port(), 0};
    sfd_.send(reinterpret_cast<char*>(&data), sizeof(data), 0);
    vstream_.stop();
  });

  p_.insert(7, [&] {
    uint16_t op = 7;
    sfd_.send(reinterpret_cast<char*>(op), sizeof(op), 0);
  });

  p_.insert(8, [&] {
    Data data{8, chErase_};
    sfd_.send(reinterpret_cast<char*>(&data),
              sizeof(uint16_t) + sizeof(uint64_t), 0);
  });
};

void Client::connect(in_addr_t ip, in_port_t port) {
  if (jt_.joinable())
    throw std::runtime_error("client: stream already is running");
  sockaddr_in sv;
  std::memset(&sv, 0, sizeof(sockaddr_in));

  sv.sin_family = AF_INET;
  sv.sin_addr.s_addr = ip;
  sv.sin_port = port;

  sfd_.create(AF_INET, SOCK_STREAM, 0);
  sfd_.connect(reinterpret_cast<sockaddr*>(&sv), sizeof(sockaddr_in));

  epoll_event ev;
  ev.events = EPOLLIN | EPOLLOUT | EPOLLERR;
  ep_.insert(sfd_, ev);

  jt_ = std::jthread([&](std::stop_token stok) { runLoop(stok); });
}

void Client::disconnect() noexcept {
  vstream_.stop();
  sfd_.close();
}

void Client::insertChannel() {
  epoll_event ev;
  if (ep_.wait(&ev, 1, 0)) {
    if (ev.events & EPOLLOUT)
      p_.execute(7);
  }
}

void Client::eraseChannel(uint64_t cid) {
  epoll_event ev;
  if (ep_.wait(&ev, 1, 0)) {
    if (ev.events & EPOLLOUT) {
      chErase_ = cid;
      p_.execute(8);
    }
  }
}

void Client::connectStream(uint64_t cid) {
  epoll_event ev;
  if (ep_.wait(&ev, 1, 0)) {
    if (ev.events & EPOLLOUT) {
      vstream_ = cid;
      p_.execute(5);
    }
  }
}

void Client::disconnectStream() {
  epoll_event ev;
  if (ep_.wait(&ev, 1, 0)) {
    if (ev.events & EPOLLOUT)
      p_.execute(6);
  }
}

bool Client::isConnectedStream() const noexcept {
  return vstream_.joinable();
}

void Client::runLoop(std::stop_token stok) {
  try {
    std::string data;

    p_.insert(10, [&] {
      if ((data.length() - 2) % sizeof(uint16_t) != 0)
        throw std::out_of_range("client (loop): incorrect receive message");
      uint64_t chId;
      svdata_.clear();
      svdata_.reserve((data.size() - 2) / sizeof(uint64_t));
      for (size_t i = 2; i < data.length(); i += sizeof(uint64_t)) {
        std::memcpy(&chId, data.data() + i, sizeof(uint64_t));
        svdata_.insert(chId);
      }
    });
    p_.insert(11, [&] {
      if (data.length() != 4)
        throw std::out_of_range("client (loop): incorrect receive message");
      uint16_t port;
      std::memcpy(&port, data.data() + 2, 2);
      vstream_.run(sfd_.ip<true>(), port);
    });

    epoll_event ev;
    uint16_t op;

    while (!stok.stop_requested()) {
      if (ep_.wait(&ev, 1, 20)) {
        if (ev.events & EPOLLIN) {
          try {
            data = sfd_.recv();
            if (data.length() >= 2) {
              std::memcpy(&op, data.data(), 2);
              p_.execute(op);
            }
          } catch (const std::exception& e) {
            std::osyncstream(std::cerr) << e.what() << '\n';
          }
        }
        if (ev.events & EPOLLERR) {
          disconnect();
          std::osyncstream(std::cerr) << "client (loop): socket closed\n";
        }
        if (ev.events & EPOLLOUT) {
          std::this_thread::sleep_for(20ms);
        }
      }
    }
  } catch (const std::exception& e) {
    std::osyncstream(std::cerr) << e.what() << '\n';
  }
}
