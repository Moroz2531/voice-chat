#pragma once

#include <tbb/concurrent_hash_map.h>
#include <boost/lockfree/queue.hpp>
#include <span>
#include <thread>
#include <vector>

#include <epoll.hpp>
#include <iserver.hpp>
#include <user.hpp>

namespace messenger {

struct MainServerParams {};

class MainServer final : public BaseServer {
  using ConcurrentHashMapUsers =
      tbb::concurrent_hash_map<std::u16string, std::unique_ptr<IUser>>;
  using ConcurrentHashMapServers =
      tbb::concurrent_hash_map<uint64_t, std::unique_ptr<IServer>>;
  using ConcurrentHashMapGuests = tbb::concurrent_hash_map<int, Guest>;

  class Acceptor final {
   public:
    Acceptor(size_t cnt = 1);

   public:
    int wait(std::span<epoll_event> evs, int timeout_ms) {
      return ep_.wait(evs.data(), evs.size(), timeout_ms);
    }
    bool insert(Socket&& sfd);
    const std::vector<std::unique_ptr<Socket>>& get() const noexcept {
      return accpt_;
    }
    const ConcurrentHashMapGuests& guests() const noexcept { return gsts_; }

   private:
    std::vector<std::unique_ptr<Socket>> accpt_;
    ConcurrentHashMapGuests gsts_;
    Epoll ep_;
  };

  class Users final {
   public:
    Users() = default;

   public:
    int wait(std::span<epoll_event> evs, int timeout_ms) {
      return ep_.wait(evs.data(), evs.size(), timeout_ms);
    }
    const ConcurrentHashMapUsers& get() const noexcept { return usrs_; }

   private:
    ConcurrentHashMapUsers usrs_;
    Epoll ep_;
  };

 public:
  MainServer(uint64_t id);

 public:
  bool local_ip(in_addr_t& addr) const noexcept;
  bool local_port(in_port_t& port) const noexcept;
  size_t count_users() const noexcept { return usrs_.get().size(); }

 private:
  void handler_users(std::stop_token stok);
  void handler_accept(std::stop_token stok);

 private:
  std::jthread jt_;
  Acceptor acc_;
  Users usrs_;
  ConcurrentHashMapServers srvs_;
};

};  // namespace messenger
