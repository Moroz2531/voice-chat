#pragma once

#include <netinet/in.h>
#include <thread>
#include <unordered_map>

#include "general/parse.hpp"
#include "lib/epoll.hpp"
#include "lib/socket.hpp"
#include "server.hpp"

namespace server {

class MainServer {
   public:
    MainServer();

   public:
    void run();
    void stop();
    bool joinable() const noexcept;

   public:
    in_addr_t ip() const;
    in_port_t port() const;

   private:
    void runLoop(std::stop_token stok);

   private:
    std::jthread jt_;
    containers::Socket sfdAccpt_;
    Server sv{0};
};

}  // namespace server
