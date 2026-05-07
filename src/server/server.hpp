#pragma once

#include <memory>
#include <mutex>
#include <thread>
#include <unordered_map>

#include "channel.hpp"
#include "general/parse.hpp"
#include "lib/epoll.hpp"
#include "lib/socket.hpp"

namespace server {
struct DataUser {
  containers::Socket sfd;
  uint64_t chsSize{0};
  uint64_t channelIndex{0};
  bool connectChannel{false}, procReq{false};

  void operator=(const containers::Socket& fd) { sfd = fd; }
  void operator=(size_t size) noexcept { chsSize = size; }

  explicit operator containers::Socket() { return sfd; }
  operator size_t() const noexcept { return chsSize; }
};

struct ParseInfo {
  std::string_view data;
  std::shared_ptr<DataUser>& du;
};

class Server {
  enum Options {
    MAX_EVENTS = 50,
    TIMEOUT_MS = 1,
  };

 public:
  Server(size_t id);
  Server(const Server& other) = delete;
  Server(Server&& other) noexcept = delete;
  ~Server() { stop(); }

  Server& operator=(const Server& rhs) = delete;
  Server& operator=(Server&& rhs) noexcept = delete;

 public:
  operator uint64_t() const noexcept { return id_; }

 public:
  void run();
  void stop();
  bool joinable() const noexcept;

 public:
  void insert(const containers::Socket& sfd);
  void erase(const containers::Socket& sfd);

  size_t size() const;
  bool empty() const;
  bool contains(const containers::Socket& sfd) const;

 private:
  void runLoop(std::stop_token stok);
  void fillParse(containers::Parse& p, ParseInfo& pinfo);

 private:
  size_t id_;
  std::jthread jt_;
  containers::Epoll ep_;
  uint64_t chIdx_{0};
  std::unordered_map<uint64_t, std::unique_ptr<Channel>> chs_;
  std::unordered_map<int, std::shared_ptr<DataUser>> sfds_;
};

}  // namespace server
