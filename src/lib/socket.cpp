#if defined _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <errno.h>
#include <unistd.h>
#endif

#include <algorithm>
#include <cstring>
#include <stdexcept>
#include <thread>

#include "socket.hpp"

using namespace containers;

Socket::Socket(int domain, int type, int protocol) : refCount_{std::make_shared<std::atomic_size_t>(1)} {
#ifdef _WIN32
    if (::WSAStartup(MAKEWORD(2, 2), &wsaData_) != 0)
        throw std::runtime_error("WSAStartup");
    if ((sfd_ = ::socket(domain, type, protocol)) == ~0) {
        ::WSACleanup();
        throw std::runtime_error("socket: create socket\n");
    }
#else
    if ((sfd_ = ::socket(domain, type, protocol)) == -1)
        throw std::runtime_error("socket: create socket\n");
#endif
}

Socket::Socket(int fd) : sfd_{fd}, refCount_{std::make_shared<std::atomic_size_t>(1)} {
#ifdef _WIN32
    if (::WSAStartup(MAKEWORD(2, 2), &wsaData_) != 0)
        throw std::runtime_error("WSAStartup");
#endif
}

Socket::~Socket() {
    close();
}

Socket::Socket(const Socket& sock) : sfd_{sock.sfd_}, refCount_{sock.refCount_} {
    if (isValid()) {
#ifdef _WIN32
        if (::WSAStartup(MAKEWORD(2, 2), &wsaData_) != 0)
            throw std::runtime_error("WSAStartup");
#endif
        ++(*refCount_);
    }
}

Socket::Socket(Socket&& sock) noexcept : refCount_{sock.refCount_} {
#ifdef _WIN32
    wsaData_ = sock.wsaData_;
    sfd_ = std::exchange(sock.sfd_, ~0);
#else
    sfd_ = std::exchange(sock.sfd_, -1);
#endif
}

Socket& Socket::operator=(const Socket& sock) {
    if (this != &sock) {
        Socket temp{sock};
        swap(temp);
    }
    return *this;
}

Socket& Socket::operator=(Socket&& sock) noexcept {
    if (this != &sock)
        swap(sock);
    return *this;
}

bool Socket::isValid() const noexcept {
#ifdef _WIN32
    if (sfd_ == ~0)
#else
    if (sfd_ == -1)
#endif
        return false;
    return true;
}

int Socket::getType() const {
    int sock_type;
    socklen_t len = sizeof(sock_type);

    getsockopt(SOL_SOCKET, SO_TYPE, &sock_type, &len);

    return sock_type;
}

void Socket::create(int domain, int type, int protocol) {
    Socket temp{domain, type, protocol};
    swap(temp);
}

#if _WIN32
SOCKET Socket::getRaw() const {
    return sfd_;
}
#else
int Socket::getRaw() const {
    return sfd_;
}
#endif

void Socket::listen(int queue) const {
    if (::listen(sfd_, queue))
        throw std::runtime_error("listen");
}

Socket Socket::accept(sockaddr* addr, socklen_t* addrLen) const {
    int sfd;
    do {
        if ((sfd = ::accept(sfd_, addr, addrLen)) == -1) {
            if (errno == ECONNABORTED || errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK) {
                std::this_thread::yield();
                continue;
            }
            throw std::runtime_error("accept");
        }
    } while (sfd == -1);
    try {
        return Socket(sfd);
    } catch (const std::exception& e) {
        ::close(sfd);
        throw;
    }
}

void Socket::close() noexcept {
#ifdef _WIN32
    if (sfd_ != ~0) {
        ::WSACleanup();
        if (--(*refCount_) == 0) {
            ::closesocket(sfd_);
            refCount_.reset();
        }
        sfd_ = ~0;
    }
#else
    if (sfd_ != -1) {
        if (--(*refCount_) == 0) {
            ::close(sfd_);
            refCount_.reset();
        }
        sfd_ = -1;
    }
#endif
}

void Socket::bind(const sockaddr* addr, socklen_t addrLen) const {
    int retval = ::bind(sfd_, addr, addrLen);

#ifdef _WIN32
    if (retval == SOCKET_ERROR)
#else
    if (retval == -1)
#endif
        throw std::runtime_error("bind");
}

void Socket::getsockname(sockaddr* addr, socklen_t* addrLen) const {
    int retval = ::getsockname(sfd_, addr, addrLen);

#ifdef _WIN32
    if (retval == SOCKET_ERROR)
#else
    if (retval == -1)
#endif
        throw std::runtime_error("getsockname");
}

void Socket::getpeername(sockaddr* addr, socklen_t* addrLen) const {
    int retval = ::getpeername(sfd_, addr, addrLen);

#ifdef _WIN32
    if (retval == SOCKET_ERROR)
#else
    if (retval == -1)
#endif
        throw std::runtime_error("getpeername");
}

void Socket::connect(const sockaddr* addr, socklen_t addrLen) const {
    int retval = ::connect(sfd_, addr, addrLen);

#ifdef _WIN32
    if (retval == SOCKET_ERROR)
#else
    if (retval == -1)
#endif
        throw std::runtime_error("connect");
}

netsize_t Socket::sendto(const char* buf, size_t count, int flags, const sockaddr* addr, socklen_t addrLen) const {
    netsize_t sendBytes = ::sendto(sfd_, buf, count, flags, addr, addrLen);
#ifdef _WIN32
    if (sendBytes == SOCKET_ERROR) {
        if (WSAGetLastError() == WSAEWOULDBLOCK)
            return 0;
        throw std::runtime_error("sendto");
    }
#else
    if (sendBytes == -1) {
        if (errno == EAGAIN || errno == EWOULDBLOCK)
            return 0;
        throw std::runtime_error("sendto");
    }
#endif
    return sendBytes;
}

netsize_t Socket::recvfrom(char* buf, size_t count, int flags, sockaddr* addr, socklen_t* addrLen) const {
    netsize_t recvBytes = ::recvfrom(sfd_, buf, count, flags, addr, addrLen);
#ifdef _WIN32
    if (recvBytes == SOCKET_ERROR) {
        if (WSAGetLastError() == WSAEWOULDBLOCK)
            return 0;
        throw std::runtime_error("recvfrom");
    }
#else
    if (recvBytes == -1) {
        if (errno == EAGAIN || errno == EWOULDBLOCK)
            return 0;
        throw std::runtime_error("recvfrom");
    }
#endif
    return recvBytes;
}

netsize_t Socket::sendto(const float* buf, size_t count, int flags, const sockaddr* addr, socklen_t addrLen) const {
    size_t bytes = count * sizeof(float);
    const char* index = reinterpret_cast<const char*>(buf);
    netsize_t sendBytes = sendto(index, bytes, flags, addr, addrLen);

    return sendBytes / sizeof(float);
}

netsize_t Socket::recvfrom(float* buf, size_t count, int flags, sockaddr* addr, socklen_t* addrLen) const {
    const size_t bytes{count * sizeof(float)};
    char tempbuf[DATA_BYTES_MAX_LEN];
    size_t size;
    netsize_t recvBytes;
    size_t offset{0};

    do {
        size = std::min(bytes - offset, static_cast<size_t>(DATA_BYTES_MAX_LEN));
        recvBytes = recvfrom(tempbuf, size, flags, addr, addrLen);
        if (recvBytes)
            std::memcpy(buf + (offset / sizeof(float)), tempbuf, recvBytes);
        offset += recvBytes;
    } while (recvBytes != 0 && offset < bytes);
    return offset / sizeof(float);
}

inline size_t Socket::cmp(size_t n1, size_t n2) const {
    return (n1 < n2) ? n1 : n2;
}

void Socket::setsockopt(int level, int optname, const void* optval, socklen_t optlen) const {
    int retval = ::setsockopt(sfd_, level, optname, optval, optlen);

#ifdef _WIN32
    if (retval == SOCKET_ERROR)
#else
    if (retval == -1)
#endif
        throw std::runtime_error("setsockopt");
}

void Socket::getsockopt(int level, int optname, void* optval, socklen_t* optlen) const {
    int retval = ::getsockopt(sfd_, level, optname, optval, optlen);

#ifdef _WIN32
    if (retval == SOCKET_ERROR)
#else
    if (retval == -1)
#endif
        throw std::runtime_error("getsockopt");
}

netsize_t Socket::send(const char* buf, size_t count, int flags) const {
    netsize_t sendBytes = ::send(sfd_, buf, count, flags);

#ifdef _WIN32
    if (sendBytes == SOCKET_ERROR) {
        if (WSAGetLastError() == WSAEWOULDBLOCK)
            return 0;
        throw std::runtime_error("send");
    }
#else
    if (sendBytes == -1) {
        if (errno == EAGAIN || errno == EWOULDBLOCK)
            return 0;
        throw std::runtime_error("send");
    }
#endif
    return sendBytes;
}

netsize_t Socket::recv(char* buf, size_t count, int flags) const {
    netsize_t recvBytes = ::recv(sfd_, buf, count, flags);

#ifdef _WIN32
    if (recvBytes == SOCKET_ERROR) {
        if (WSAGetLastError() == WSAEWOULDBLOCK)
            return 0;
        throw std::runtime_error("recv");
    }
#else
    if (recvBytes == -1) {
        if (errno == EAGAIN || errno == EWOULDBLOCK)
            return 0;
        throw std::runtime_error("recv");
    }
#endif
    return recvBytes;
}

void Socket::swap(Socket& sock) noexcept {
#ifdef _WIN32
    std::swap(wsaData_, sock.wsaData_);
#endif
    std::swap(sfd_, sock.sfd_);
    std::swap(refCount_, sock.refCount_);
}

netsize_t Socket::send(const std::string& buf, int flags) const {
    return Socket::send(buf.c_str(), buf.length(), flags);
}

std::string Socket::recv(int flags) const {
    std::string bufRes;
    char buf[DATA_BYTES_MAX_LEN];
    netsize_t cbytes;
    bufRes.reserve(DATA_BYTES_MAX_LEN);

    while ((cbytes = Socket::recv(buf, DATA_BYTES_MAX_LEN - 1, flags))) {
        buf[cbytes] = '\0';
        bufRes += buf;
    }
    return bufRes;
}
