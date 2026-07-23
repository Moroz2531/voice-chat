#include <netinet/in.h>
#include <boost/container/static_vector.hpp>
#include <cstring>
#include <iostream>
#include <stdexcept>

#include "channel.hpp"

using namespace messenger;

namespace {
constexpr uint32_t DEFAULT_PARAMS_EPOLL =
    EPOLLIN | EPOLLET | EPOLLERR | EPOLLHUP;
}

void VoiceChannel::insert(in_addr_t addr, in_port_t port) {
  usrs_.insert(
      std::pair<std::pair<in_addr_t, in_port_t>, double>{{addr, port}, 0});
}

void VoiceChannel::erase(in_addr_t addr, in_port_t port) {
  usrs_.erase(std::pair<in_addr_t, in_port_t>{addr, port});
}

size_t VoiceChannel::size() const noexcept {
  return usrs_.size();
}

bool VoiceChannel::empty() const noexcept {
  return usrs_.empty();
}

void VoiceChannel::clear() noexcept {
  usrs_.clear();
}

bool VoiceChannel::ip(in_addr_t& addr) const noexcept {
  if (!sfd_.get())
    return false;
  return true;
}

bool VoiceChannel::port(in_port_t& port) const noexcept {
  if (!sfd_.get())
    return false;
  port = sfd_->local_port();
  return true;
}

void VoiceChannel::exec(std::stop_token stok) {
  auto handleInput = [&](std::span<char> data) {
    sockaddr_in sin;
    socklen_t slen = sizeof(sin);
    netsize_t bytes;
    std::memset(&sin, 0, slen);
    while ((bytes = sfd_->recvfrom(data.data(), data.size(), 0,
                                   reinterpret_cast<sockaddr*>(&sin), &slen))) {
      HashMap::accessor acc;
      std::pair<in_addr_t, in_port_t> curKey{sin.sin_addr.s_addr, sin.sin_port};
      if (!usrs_.find(acc, curKey))
        continue;
      for (auto&& i : usrs_) {
        if (i.first == curKey)
          continue;
        auto tsin = createSockaddrIn(AF_INET, i.first.first, i.first.second);
        constexpr auto gb = 1024 * 1024 * 1024;
        i.second += static_cast<double>(bytes) / gb;
        sfd_->sendto(data.data(), bytes, 0, reinterpret_cast<sockaddr*>(&tsin),
                     sizeof(tsin));
      }
    }
  };

  try {
    sfd_ = std::make_unique<Socket>(AF_INET, SOCK_DGRAM | SOCK_NONBLOCK, 0);
    sockaddr_in sin = createSockaddrIn(AF_INET, 0, INADDR_ANY);

        boost::container::static_vector<char, 1400> data;
    Epoll ep;
    epoll_event ev{.events = DEFAULT_PARAMS_EPOLL, .data.fd = *sfd_};
    ep.insert(*sfd_, ev);

    en_.exchange(true);
    while (!stok.stop_requested()) {
      if (ep.wait(&ev, 1, 200)) {
        if (ev.events & EPOLLIN)
          handleInput(std::span{data.data(), data.max_size()});
        if (ev.events & (EPOLLERR | EPOLLHUP))
          break;
      }
    }
  } catch (...) {
  }
  en_.exchange(false);
  sfd_.reset();
}
