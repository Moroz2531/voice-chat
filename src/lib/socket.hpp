#pragma once

#include <atomic>
#include <memory>
#include <utility>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2def.h>
#include <ws2tcpip.h>

using netsize_t = int;
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>

using netsize_t = ssize_t;
#endif

namespace containers {

#define DATA_BYTES_MAX_LEN 1458
#define DATA_FLOAT_LEN (DATA_BYTES_MAX_LEN / sizeof(float))

class Socket final {
  using Counter = std::shared_ptr<std::atomic_size_t>;
  enum SocketOptions {
    LISTEN_QUEUE_LEN = 10,
  };
#ifdef _WIN32
  WSADATA wsaData_;
  SOCKET sfd_{~0};
#else
  int sfd_{-1};
#endif
  Counter refCount_;

 public:
  Socket() = default;
  Socket(int domain, int type, int protocol);
  Socket(const Socket& sock);
  Socket(Socket&& sock) noexcept;
  virtual ~Socket();

  Socket& operator=(const Socket& sock);
  Socket& operator=(Socket&& sock) noexcept;

  int getType() const;

  void create(int domain, int type, int protocol);
  void close() noexcept;

#ifdef _WIN32
  SOCKET getRaw() const;
#else
  int getRaw() const;
#endif

  void bind(const sockaddr* addr, socklen_t addrLen) const;
  void listen(int queue = LISTEN_QUEUE_LEN) const;
  void connect(const sockaddr* addr, socklen_t addrLen) const;

  Socket accept(sockaddr* addr = nullptr, socklen_t* addrLen = nullptr) const;
  Socket accept4(sockaddr* addr = nullptr,
                 socklen_t* addrLen = nullptr,
                 int flags = 0) const;

  void getsockname(sockaddr* addr, socklen_t* addrLen) const;
  void getpeername(sockaddr* addr, socklen_t* addrLen) const;

  void setsockopt(int level,
                  int optname,
                  const void* optval,
                  socklen_t optlen) const;
  void getsockopt(int level,
                  int optname,
                  void* optval,
                  socklen_t* optlen) const;

  netsize_t send(const std::string_view buf, int flags = 0) const;
  netsize_t send(const char* buf, size_t count, int flags) const;
  netsize_t send(const float* buf, size_t count, int flags) const;
  netsize_t sendto(const char* buf,
                   size_t count,
                   int flags,
                   const sockaddr* addr,
                   socklen_t addrLen = 0) const;
  netsize_t sendto(const float* buf,
                   size_t count,
                   int flags,
                   const sockaddr* addr,
                   socklen_t addrLen = 0) const;

  std::string recv(int flags = 0) const;
  netsize_t recv(char* buf, size_t count, int flags) const;
  netsize_t recvfrom(char* buf,
                     size_t count,
                     int flags,
                     sockaddr* addr,
                     socklen_t* addrLen) const;
  netsize_t recvfrom(float* buf,
                     size_t count,
                     int flags,
                     sockaddr* addr,
                     socklen_t* addrLen) const;

  template <bool remote = false>
  in_addr_t ip() const {
    sockaddr_in sin;
    socklen_t len = sizeof(sockaddr_in);
    if constexpr (!remote)
      getsockname(reinterpret_cast<sockaddr*>(&sin), &len);
    else
      getpeername(reinterpret_cast<sockaddr*>(&sin), &len);
    return sin.sin_addr.s_addr;
  }

  template <bool remote = false>
  in_port_t port() const {
    sockaddr_in sin;
    socklen_t len = sizeof(sockaddr_in);
    if constexpr (!remote)
      getsockname(reinterpret_cast<sockaddr*>(&sin), &len);
    else
      getpeername(reinterpret_cast<sockaddr*>(&sin), &len);
    return sin.sin_port;
  }

  bool isValid() const noexcept;

  operator auto() const { return sfd_; }

 private:
#ifdef __linux__
  explicit Socket(int fd);
#endif
  void swap(Socket& sock) noexcept;
};

}  // namespace containers
