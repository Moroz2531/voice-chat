#pragma once

#include <atomic>
#include <boost/unordered_set.hpp>
#include <thread>

#include "general/parse.hpp"
#include "lib/epoll.hpp"
#include "lib/socket.hpp"

namespace voicechat {

class VoiceStream {
 public:
  VoiceStream();

 public:
  void run(in_addr_t ip, in_port_t port);
  void stop();
  bool joinable() const noexcept;

 public:
  uint16_t port() const { return sfd_.port(); }

  operator uint64_t() const noexcept { return cid_; }
  void operator=(uint64_t cid) noexcept { cid_ = cid; }

  void runInputDevice() { inDev_ = true; }
  void stopInputDevice() { inDev_ = false; }

  void runOutputDevice() { outDev_ = true; }
  void stopOutputDevice() { outDev_ = false; }

 private:
  void runLoop(std::stop_token stok);

 private:
  std::jthread jt_;
  uint64_t cid_{0};
  containers::Socket sfd_;
  containers::Epoll ep_;
  std::atomic_bool inDev_{true}, outDev_{true};
};

class Client {
 public:
  using ServerData = boost::unordered_set<uint64_t>;

  Client();

 public:
  bool connect(in_addr_t ip, in_port_t port);
  void disconnect() noexcept;

  void insertChannel();
  void eraseChannel(uint64_t id);

  void connectStream(uint64_t id);
  void disconnectStream();
  bool isConnectedStream() const noexcept;

  void runInputDevice() { vstream_.runInputDevice(); }
  void stopInputDevice() { vstream_.stopInputDevice(); }

  void runOutputDevice() { vstream_.runOutputDevice(); }
  void stopOutputDevice() { vstream_.stopOutputDevice(); }

 public:
  size_t sizeChannels() const noexcept { return svdata_.size(); }
  const ServerData& channels() const noexcept { return svdata_; }

 private:
  void runLoop(std::stop_token stok);

 private:
  mutable std::mutex mut_;
  std::jthread jt_;
  containers::Socket sfd_;
  containers::Epoll ep_;
  containers::Parse p_;
  ServerData svdata_;
  uint64_t chErase_;
  VoiceStream vstream_;
};

}  // namespace voicechat
