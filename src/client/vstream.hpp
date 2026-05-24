#pragma once

#include <boost/lockfree/spsc_queue.hpp>
#include <cstring>

#include "lib/socket.hpp"

namespace voicechat {

template <typename T, size_t Size>
class Transmitter final {
  constexpr auto MTU = 1500;
  constexpr auto MAX_SEND_BYTES = MTU - sizeof(iphdr) - sizeof(udphdr);
  constexpr auto MAX_RECV_BYTES = MTU;

  constexpr auto MAX_SEND = MAX_SEND_BYTES - MAX_SEND_BYTES % sizeof(T);
  constexpr auto MAX_RECV = MAX_RECV_BYTES - MAX_RECV_BYTES % sizeof(T);

 public:
  Transmitter(const Socket& sfd)
      : sfd_{sfd},
        dataSend_{new char[MAX_SEND]},
        dataRecv_{new char[MAX_RECV]} {}
  Transmitter(const Transmitter& other) = delete;
  Transmitter(const Transmitter&& other) noexcept = delete;
  ~Transmitter() {
    delete[] dataSend_;
    delete[] dataRecv_;
  }

 public:
  Transmitter& operator=(const Transmitter& rhs) = delete;
  Transmitter& operator=(Transmitter&& rhs) noexcept = delete;

  void operator=(const Socket& sfd) { sfd_ = sfd; }

 public:
  bool push(const T& t) { in_.push(t); }
  size_t push(const T* t, size_t size) { return in_.push(t, size); }

  bool pop(T& t) { return out_.pop(t); }
  size_t pop(T* t, size_t size) { return out_.pop(t, size); }

  ssize_t send() { return send(std::numeric_limits<std::size_t>::max()); }
  ssize_t send(size_t count) {
    count = std::min(count, in_.read_available());
    size_t accessSend = 0;

    do {
      auto readyCount =
          in_.pop(dataSend_, std::min(count, MAX_SEND / sizeof(T)));
      auto bytes = sfd_.send(dataSend_, readyCount * sizeof(T), 0);
      if (bytes == -1)
        return accessSend ? accessSend : -1;
      accessSend += bytes / sizeof(T);
    } while (accessSend != count);
    return accessSend;
  }

  ssize_t recv() { return recv(std::numeric_limits<std::size_t>::max()); }
  ssize_t recv(size_t count) {
    auto available = sfd_.recv(nullptr, 0, MSG_PEEK | MSG_TRUNC);
    if (available == -1)
      return -1;
    auto availableWrite = std::min(count, out_.write_available());
    auto maxBytes =
        std::min(availableWrite * sizeof(T), static_cast<size_t>(available));
    if (!maxBytes)
      return 0;

    size_t off{};
    T t;
    do {
      netsize_t curBytes =
          sfd_.recv(dataRecv_, std::min(MAX_RECV, maxBytes - off), 0);
      if (curBytes == -1)
        return off ? off / sizeof(T) : -1;
      for (netsize_t i = 0; i < curBytes; i += sizeof(T)) {
        std::memcpy(&t, dataRecv_ + i, sizeof(T));
        out_.push(t);
      }
      off += curBytes;
    } while (off != maxBytes);
    return off / sizeof(T);
  }

 private:
  Socket sfd_;
  boost::lockfree::spsc_queue<T, boost::lockfree::capacity<Size>> in_, out_;
  char *dataSend_, *dataRecv_;
};

}  // namespace voicechat
