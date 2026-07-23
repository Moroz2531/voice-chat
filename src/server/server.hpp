#pragma once

#include <tbb/concurrent_hash_map.h>
#include <tbb/concurrent_set.h>
#include <atomic>
#include <memory>

#include <channel.hpp>
#include <general/parse.hpp>
#include <iserver.hpp>
#include <lib/socket.hpp>
#include <user.hpp>
#include <vector>

namespace messenger {

struct ServerLoopSettings {
  const int maxEvents{50}, timeoutMS{100};
};

struct StatusServer {
  ServerLoopSettings slsettings;
  bool enable;
};

using ServerUsersHashMap =
    tbb::concurrent_hash_map<int, std::unique_ptr<IUser>>;
using ServerChannelsHashMap =
    tbb::concurrent_hash_map<uint64_t, std::unique_ptr<IChannel>>;

class Server final : public BaseServer, private ServerLoopSettings {
 public:
  Server(size_t id) : BaseServer(id) {};

 public:
  bool insert(const ISocket& sfd, std::unique_ptr<IUser> u);
  bool erase(const ISocket& sfd, std::unique_ptr<IUser>& res) noexcept;

  size_t size() const noexcept { return conUsers_.size(); };
  bool empty() const noexcept { return conUsers_.empty(); };

  StatusServer get() const;

  void exec(std::stop_token stok) override final;

 private:
  std::atomic_bool en_{false};
  ServerChannelsHashMap chs_;
  ServerUsersHashMap conUsers_;
  Epoll ep_;
};
}  // namespace messenger
