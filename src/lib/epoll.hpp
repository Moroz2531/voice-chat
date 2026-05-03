#pragma once

#include <sys/epoll.h>
#include <unistd.h>

#include "socket.hpp"

namespace containers {
class Epoll final {
   public:
    Epoll();
    Epoll(const Epoll& other) = delete;
    Epoll(Epoll&& other) noexcept;
    ~Epoll() { close(epfd_); }

    Epoll& operator=(const Epoll& rhs) = delete;
    Epoll& operator=(Epoll&& rhs) noexcept;

   public:
    void insert(int sfd, epoll_event& ev) const;
    void insert(const Socket& sfd, epoll_event& ev) const;

    void change(int sfd, epoll_event& ev) const;
    void change(const Socket& sfd, epoll_event& ev) const;

    void erase(int sfd) const;
    void erase(const Socket& sfd) const;

    int wait(epoll_event* evs, int maxevs, int timeout_ms) const;

    operator int() const { return epfd_; }

   private:
    int epfd_;
};
}  // namespace containers
