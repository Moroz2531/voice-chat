#pragma once

#include <memory>
#include <mutex>
#include <regex>
#include <thread>
#include <unordered_map>

#include <general/parse.hpp>
#include "channel.hpp"
#include "lib/epoll.hpp"
#include "lib/socket.hpp"
#include "mainserver.hpp"

namespace server {
struct DataUser {
    containers::Socket sfd;
    size_t chsSize{0};

    void operator=(const containers::Socket& fd) { sfd = fd; }
    void operator=(size_t size) { chsSize = size; }

    explicit operator containers::Socket() { return sfd; }
    operator size_t() const { return chsSize; }
};

struct ParseInfo {
    std::smatch& match;
    containers::Socket& sfd;
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
    operator size_t() const noexcept { return id_; }

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

    std::string getChannelsIds() const;

   private:
    size_t id_;
    std::jthread jt_;
    containers::Epoll ep_;
    size_t chIdx_{0};
    std::unordered_map<size_t, std::unique_ptr<Channel>> chs_;
    std::unordered_map<int, std::shared_ptr<DataUser>> sfds_;
};

}  // namespace server
