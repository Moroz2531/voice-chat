#include <errno.h>
#include <stdexcept>
#include <utility>

#include "epoll.hpp"

using namespace containers;

Epoll::Epoll() : epfd_{epoll_create1(EPOLL_CLOEXEC)} {
  if (epfd_ == -1)
    throw std::system_error();
}

Epoll::Epoll(Epoll&& other) noexcept : epfd_{std::exchange(other.epfd_, -1)} {}

Epoll& Epoll::operator=(Epoll&& rhs) noexcept {
  if (this != &rhs)
    std::swap(epfd_, rhs.epfd_);
  return *this;
}

void Epoll::insert(const Socket& sfd, epoll_event& ev) const {
  insert(static_cast<int>(sfd), ev);
}

void Epoll::change(const Socket& sfd, epoll_event& ev) const {
  change(static_cast<int>(sfd), ev);
}

void Epoll::erase(const Socket& sfd) const {
  erase(static_cast<int>(sfd));
}

void Epoll::insert(int sfd, epoll_event& ev) const {
  if (epoll_ctl(epfd_, EPOLL_CTL_ADD, sfd, &ev))
    throw(std::system_error());
}

void Epoll::change(int sfd, epoll_event& ev) const {
  if (epoll_ctl(epfd_, EPOLL_CTL_MOD, sfd, &ev))
    throw(std::system_error());
}

void Epoll::erase(int sfd) const {
  epoll_event temp;
  if (epoll_ctl(epfd_, EPOLL_CTL_DEL, sfd, &temp)) {
    if (errno == ENOENT)
      return;
    throw(std::system_error());
  }
}

int Epoll::wait(epoll_event* evs, int maxevs, int timeout_ms) const {
  int retval = epoll_wait(epfd_, evs, maxevs, timeout_ms);
  if (retval == -1) {
    if (errno == EINTR)
      return 0;
    else
      throw(std::system_error());
  }
  return retval;
}
