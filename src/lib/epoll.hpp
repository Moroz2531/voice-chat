#pragma once

#include <sys/epoll.h>
#include <unistd.h>
#include <concepts>
#include <type_traits>

#include "socket.hpp"

namespace messenger {
class Epoll final {
 public:
  Epoll();
  Epoll(const Epoll& other) = delete;
  Epoll(Epoll&& other) noexcept;
  ~Epoll() { close(epfd_); }

  Epoll& operator=(const Epoll& rhs) = delete;
  Epoll& operator=(Epoll&& rhs) noexcept;

 public:
  template <typename T>
    requires std::is_same_v<std::remove_cvref_t<T>, epoll_event>
  bool insert(const Socket& sfd, T&& ev) const {
    return insert(static_cast<int>(sfd), std::forward<T>(ev));
  }

  void change(int sfd, epoll_event& ev) const;
  void change(const Socket& sfd, epoll_event& ev) const;

  void erase(int sfd) const;
  void erase(const Socket& sfd) const;

  int wait(epoll_event* evs, int maxevs, int timeout_ms) const;

  operator int() const { return epfd_; }

 private:
  bool insert(int sfd, epoll_event& ev) const;

 private:
  int epfd_;
};
}  // namespace messenger
