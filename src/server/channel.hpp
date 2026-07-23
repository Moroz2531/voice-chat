#pragma once

#include <netinet/in.h>
#include <tbb/concurrent_hash_map.h>
#include <atomic>
#include <optional>
#include <shared_mutex>
#include <span>
#include <stop_token>

#include "lib/epoll.hpp"
#include "lib/socket.hpp"

namespace messenger {

class IChannel {
  virtual uint64_t id() const noexcept = 0;
  virtual const std::u16string& name() const noexcept = 0;
  virtual void exec(std::stop_token stok) = 0;
  virtual bool enable() const noexcept = 0;
  bool ip(in_addr_t& addr) const noexcept = 0;
  bool port(in_port_t& port) const noexcept = 0;

#ifdef false
  virtual bool send(const std::span<char> buf, int flags) = 0;
  virtual size_t recv(std::span<char> buf, int flags) = 0;
#endif
};

class Channel : public IChannel {
 public:
  Channel(size_t id, const std::u16string& name) : id_{id}, name_{name} {}

  uint64_t id() const noexcept override final { return id_; }
  const std::u16string& name() const noexcept override final { return name_; }

  void set_tid(size_t tid) { tid_ = tid; }
  const std::optional<size_t>& get_tid() const noexcept { return tid_; }

 private:
  std::optional<size_t> tid_{std::nullopt};
  uint64_t id_;
  std::u16string name_;
};

class VoiceChannel final : public Channel {
  using HashMap =
      tbb::concurrent_hash_map<std::pair<in_addr_t, in_port_t>, double>;

 public:
  VoiceChannel(size_t id, const std::u16string& name) : Channel(id, name) {}

 public:
  void insert(in_addr_t addr, in_port_t port);
  void erase(in_addr_t addr, in_port_t port);

  size_t size() const noexcept;
  bool empty() const noexcept;
  void clear() noexcept;

  void exec(std::stop_token stok) override final;

  bool ip(in_addr_t& addr) const noexcept;
  bool port(in_port_t& port) const noexcept;

  bool enable() const noexcept override final { return en_; }

 private:
  HashMap usrs_;
  std::unique_ptr<ISocket> sfd_;
  std::atomic_bool en_;
};

}  // namespace messenger
