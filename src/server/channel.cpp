#include <netinet/in.h>
#include <cstring>
#include <format>
#include <iostream>
#include <stdexcept>
#include <syncstream>

#include "channel.hpp"

using namespace server;

namespace {
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

VoiceChannel::VoiceChannel(size_t id)
    : Channel(id), sfd_{AF_INET, SOCK_DGRAM | SOCK_NONBLOCK, 0} {
  sockaddr_in sin;
  std::memset(&sin, 0, sizeof(sockaddr_in));
  sin.sin_addr.s_addr = INADDR_ANY;
  sin.sin_family = AF_INET;
  sin.sin_port = 0;
  sfd_.bind(reinterpret_cast<sockaddr*>(&sin), sizeof(sockaddr_in));
}

void VoiceChannel::insert(in_addr_t addr, in_port_t port) {
  std::lock_guard<std::mutex> lock{mut_};
  users_.insert(std::pair<in_addr_t, in_port_t>{addr, port});
}

void VoiceChannel::erase(in_addr_t addr, in_port_t port) {
  std::lock_guard<std::mutex> lock{mut_};
  users_.erase(std::pair<in_addr_t, in_port_t>{addr, port});
}

in_addr_t VoiceChannel::ip() const {
  return sfd_.ip();
}

in_port_t VoiceChannel::port() const {
  return sfd_.port();
}

void VoiceChannel::run() {
  std::lock_guard<std::mutex> lock{mut_};
  if (jt_.joinable())
    throw std::runtime_error("the channel already is running");
  jt_ = std::jthread([this](std::stop_token stok) { runLoop(stok); });
}

void VoiceChannel::stop() {
  std::lock_guard<std::mutex> lock{mut_};
  if (jt_.joinable()) {
    jt_.request_stop();
    jt_.join();
  }
}

bool VoiceChannel::joinable() const noexcept {
  return jt_.joinable();
}

bool VoiceChannel::empty() const {
  std::lock_guard<std::mutex> lock{mut_};
  return users_.empty();
}

size_t VoiceChannel::size() const {
  std::lock_guard<std::mutex> lock{mut_};
  return users_.size();
}

void VoiceChannel::clear() noexcept {
  std::lock_guard<std::mutex> lock{mut_};
  users_.clear();
}

bool VoiceChannel::contains(in_addr_t addr, in_port_t port) const {
  std::lock_guard<std::mutex> lock{mut_};
  return users_.contains(std::pair<in_addr_t, in_port_t>{addr, port});
}

void VoiceChannel::runLoop(std::stop_token stok) {
  float data[DATA_FLOAT_LEN];
  sockaddr_in sinrecv, sinsend;
  socklen_t len;

  std::memset(&sinsend, 0, sizeof(sockaddr_in));
  sinsend.sin_family = AF_INET;

  while (!stok.stop_requested()) {
    len = sizeof(sockaddr_in);
    netsize_t count;
    try {
      count = sfd_.recvfrom(data, DATA_FLOAT_LEN, 0,
                            reinterpret_cast<sockaddr*>(&sinrecv), &len);
      if (!count || len != sizeof(sockaddr_in)) {
        std::this_thread::yield();
        continue;
      }
      std::pair<in_addr_t, in_port_t> u{sinrecv.sin_addr.s_addr,
                                        sinrecv.sin_port};
      std::lock_guard<std::mutex> lock{mut_};
      auto usIt = users_.find(u);
      if (usIt == users_.end())
        continue;
      for (auto it = users_.begin(); it != usIt; ++it) {
        sinsend.sin_addr.s_addr = it->first;
        sinsend.sin_port = it->second;
        try {
          sfd_.sendto(data, count, 0, reinterpret_cast<sockaddr*>(&sinsend),
                      sizeof(sockaddr_in));
        } catch (const std::runtime_error& re) {
        }
      }
      for (auto it = ++usIt, end = users_.end(); it != end; ++it) {
        sinsend.sin_addr.s_addr = it->first;
        sinsend.sin_port = it->second;
        try {
          sfd_.sendto(data, count, 0, reinterpret_cast<sockaddr*>(&sinsend),
                      sizeof(sockaddr_in));
        } catch (const std::runtime_error& re) {
        }
      }
    } catch (const std::exception& e) {
      std::osyncstream(std::cerr) << e.what() << '\n';
    }
  }
}
