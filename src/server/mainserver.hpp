#pragma once

#include <vector>

#include "lib/socket.hpp"

namespace server {
class MainServer {
   public:
    MainServer();
    MainServer(const MainServer& other) = delete;
    MainServer(MainServer&& other) noexcept;
    ~MainServer();

    MainServer& operator=(const MainServer& rhs) = delete;
    MainServer& operator=(MainServer&& rhs) noexcept;

   public:
    void add(...&& sv);
    void remove(...);

   public:
    std::string ip() const;
    ... port() const;

   private:
    containers::Socket fdsv_;
};
}  // namespace server
