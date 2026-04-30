#if defined _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#elifdef __linux__
#include <unistd.h>
#endif

#include <cstring>
#include <stdexcept>

#include "socket.hpp"

using namespace containers;

Socket::Socket(int domain, int type, int protocol) {
  create(domain, type, protocol);
}

Socket::Socket(int fd) {
#ifdef _WIN32
  if (::WSAStartup(MAKEWORD(2, 2), &wsaData_) != 0)
    throw std::runtime_error("WSAStartup");
#endif
  sfd_ = fd;
  ++(*refCount_);
}

Socket::~Socket() { close(); }

Socket::Socket(const Socket &sock) {
  sfd_ = sock.sfd_;
  refCount_ = sock.refCount_;
  if (sock.isValid()) {
#ifdef _WIN32
    if (::WSAStartup(MAKEWORD(2, 2), &wsaData_) != 0)
      throw std::runtime_error("WSAStartup");
#endif
    ++(*refCount_);
  }
}

Socket::Socket(Socket &&sock) noexcept {
#ifdef _WIN32
  wsaData_ = sock.wsaData_;
  sfd_ = std::exchange(sock.sfd_, ~0);
#elifdef __linux__
  sfd_ = std::exchange(sock.sfd_, -1);
#endif
  refCount_ = sock.refCount_;
}

Socket &Socket::operator=(const Socket &sock) {
  if (this != &sock) {
    Socket temp{sock};
    swap(temp);
  }
  return *this;
}

Socket &Socket::operator=(Socket &&sock) noexcept {
  if (this != &sock)
    swap(sock);
  return *this;
}

bool Socket::isValid() const noexcept {
  try {
    getType();
  } catch (const std::exception &e) {
    return false;
  }
  return true;
}

int Socket::getType() const {
  int sock_type;
  socklen_t len = sizeof(sock_type);

  getsockopt(SOL_SOCKET, SO_TYPE, &sock_type, &len);

  return sock_type;
}

void Socket::create(int domain, int type, int protocol) {
  if (isValid())
    throw std::invalid_argument("socket created");

  refCount_ = std::make_shared<std::atomic<int>>(1);
  sfd_ = socket(domain, type, protocol);

#ifdef _WIN32
  if (::WSAStartup(MAKEWORD(2, 2), &wsaData_) != 0)
    throw std::runtime_error("WSAStartup");
  if (sfd_ == INVALID_SOCKET)
#elifdef __linux__
  if (sfd_ == -1)
#endif
    throw std::runtime_error("socket");
}

#if _WIN32
SOCKET Socket::getRaw() const { return sfd_; }
#elifdef __linux__
int Socket::getRaw() const { return sfd_; }
#endif

void Socket::listen(int queue) const {
  if (::listen(sfd_, queue))
    throw std::runtime_error("listen");
}

Socket Socket::accept(sockaddr *addr, socklen_t *addrLen) const {
  int sfd;

  if ((sfd = ::accept(sfd_, addr, addrLen)))
    throw std::runtime_error("accept");

  return Socket(sfd);
}

void Socket::close() {
  if (isValid()) {
#ifdef _WIN32
    ::WSACleanup();
#endif
    if (--(*refCount_) == 0) {
#ifdef _WIN32
      ::closesocket(sfd_);
      sfd_ = ~0;
#elifdef __linux__
      ::close(sfd_);
      sfd_ = -1;
#endif
    }
  }
}

void Socket::bind(const struct sockaddr *addr, socklen_t addrLen) const {
  int retval = ::bind(sfd_, addr, addrLen);

#ifdef _WIN32
  if (retval == SOCKET_ERROR)
#elifdef __linux__
  if (retval == -1)
#endif
    throw std::runtime_error("bind");
}

void Socket::getsockname(sockaddr *addr, socklen_t *addrLen) const {
  int retval = ::getsockname(sfd_, addr, addrLen);

#ifdef _WIN32
  if (retval == SOCKET_ERROR)
#elifdef __linux__
  if (retval == -1)
#endif
    throw std::runtime_error("getsockname");
}

void Socket::connect(const struct sockaddr *addr, socklen_t addrLen) const {
  int retval = ::connect(sfd_, addr, addrLen);

#ifdef _WIN32
  if (retval == SOCKET_ERROR)
#elifdef __linux__
  if (retval == -1)
#endif
    throw std::runtime_error("connect");
}

netsize_t Socket::sendto(const char *buf, size_t count, int flags,
                         const sockaddr *addr, socklen_t addrLen = 0) const {
  netsize_t sendBytes = ::sendto(sfd_, buf, count, flags, addr, addrLen);
#ifdef _WIN32
  if (sendBytes == SOCKET_ERROR)
#elifdef __linux__
  if (sendBytes == -1)
#endif
    throw std::runtime_error("sendto");
  return sendBytes;
}

netsize_t Socket::recvfrom(char *buf, size_t count, int flags, sockaddr *addr,
                           socklen_t *addrLen) const {
  netsize_t recvBytes = ::recvfrom(sfd_, buf, count, flags, addr, addrLen);
#ifdef _WIN32
  if (recvBytes == SOCKET_ERROR)
#elifdef __linux__
  if (recvBytes == -1)
#endif
    throw std::runtime_error("recvfrom");
  return recvBytes;
}

netsize_t Socket::sendto(const float *buf, size_t count, int flags,
                         const sockaddr *addr, socklen_t addrLen) const {
  size_t bytes = count * sizeof(float);
  const char *index = reinterpret_cast<const char *>(buf);
  netsize_t sendBytes = sendto(index, bytes, 0, addr, addrLen);

  return sendBytes / sizeof(float);
}

netsize_t Socket::recvfrom(float *buf, size_t count, int flags, sockaddr *addr,
                           socklen_t *addrLen) const {
  size_t bytes = count * sizeof(float);
  char *index = reinterpret_cast<char *>(buf);
  netsize_t recvBytes = recvfrom(index, bytes, 0, addr, addrLen);

  return recvBytes / sizeof(float);
}

inline size_t Socket::cmp(size_t n1, size_t n2) const {
  return (n1 < n2) ? n1 : n2;
}

void Socket::setsockopt(int level, int optname, const void *optval,
                        socklen_t optlen) const {
  int retval = ::setsockopt(sfd_, level, optname, optval, optlen);

#ifdef _WIN32
  if (retval == SOCKET_ERROR)
#elifdef __linux__
  if (retval == -1)
#endif
    throw std::runtime_error("setsockopt");
}

void Socket::getsockopt(int level, int optname, void *optval,
                        socklen_t *optlen) const {
  int retval = ::getsockopt(sfd_, level, optname, optval, optlen);

#ifdef _WIN32
  if (retval == SOCKET_ERROR)
#elifdef __linux__
  if (retval == -1)
#endif
    throw std::runtime_error("getsockopt");
}

netsize_t Socket::send(const char *buf, size_t count, int flags) const {
  netsize_t sendBytes = ::send(sfd_, buf, count, flags);

#ifdef _WIN32
  if (sendBytes == SOCKET_ERROR)
#elifdef __linux__
  if (sendBytes == -1)
#endif
    throw std::runtime_error("send");
  return sendBytes;
}

netsize_t Socket::recv(char *buf, size_t count, int flags) const {
  netsize_t recvBytes = ::recv(sfd_, buf, count, flags);

#ifdef _WIN32
  if (recvBytes == SOCKET_ERROR)
#elifdef __linux__
  if (recvBytes == -1)
#endif
    throw std::runtime_error("recv");
  return recvBytes;
}

void Socket::swap(Socket &sock) noexcept {
#ifdef _WIN32
  std::swap(wsaData_, sock.wsaData_);
#endif
  std::swap(sfd_, sock.sfd_);
  std::swap(refCount_, sock.refCount_);
}
