#pragma once

#include <cstddef>
#include <cstring>
#include <memory>
#include <span>
#include <utility>
#include <vector>

#include <lib/socket.hpp>

namespace messenger {

class IUser {
 public:
  virtual ~IUser() = default;
  virtual std::unique_ptr<ISocket>& get() noexcept = 0;
  virtual const ISocket& get() const = 0;

  virtual bool send(std::span<const char> buf) = 0;
  virtual size_t recv(std::span<char> buf, char sep) = 0;
};

class BaseUser : public IUser {
 public:
  BaseUser(std::unique_ptr<ISocket> sfd) : sfd_{std::move(sfd)} {}

  std::unique_ptr<ISocket>& get() noexcept override final { return sfd_; }
  const ISocket& get() const noexcept override final { return *sfd_; }

  bool send(std::span<const char> buf) override;
  size_t recv(std::span<char> buf, char sep) override;

 private:
  std::vector<char> tempbufsend_, tempbufrecv_;
  std::unique_ptr<ISocket> sfd_;
};

class Guest final : public BaseUser {
 public:
  Guest(std::unique_ptr<ISocket> sfd) : BaseUser(std::move(sfd)) {}

 private:
};

class User final : public BaseUser {
 public:
  User(std::u16string login, std::unique_ptr<ISocket> sfd)
      : BaseUser(std::move(sfd)), login_{std::move(login)} {}
  User(std::u16string login, Guest&& gst) noexcept
      : User(std::move(login), std::move(std::move(gst.get()))) {}

 public:
  const std::u16string& login() const noexcept { return login_; }

 private:
  std::u16string login_;
};

}  // namespace messenger
