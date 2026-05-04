#pragma once

#include <netinet/in.h>
#include <boost/unordered_set.hpp>
#include <mutex>
#include <string>
#include <thread>

#include "lib/epoll.hpp"
#include "lib/socket.hpp"

namespace server {

class Channel {
   public:
    Channel(size_t id) : id_{id} {}
    Channel(const Channel& other) = default;
    Channel(Channel&& other) noexcept = default;
    virtual ~Channel() = default;

    Channel& operator=(const Channel& rhs) = default;
    Channel& operator=(Channel&& rhs) noexcept = default;

    operator size_t() const { return id_; }

   protected:
    size_t id_;
};

class VoiceChannel final : public Channel {
   public:
    VoiceChannel(size_t id);
    VoiceChannel(const VoiceChannel& other) = delete;
    VoiceChannel(VoiceChannel&& other) noexcept = delete;
    ~VoiceChannel() { stop(); }

    VoiceChannel& operator=(const VoiceChannel& rhs) = delete;
    VoiceChannel& operator=(VoiceChannel&& rhs) noexcept = delete;

   public:
    void run();
    void stop();
    bool joinable() const noexcept;

   public:
    void insert(in_addr_t addr, in_port_t port);
    void erase(in_addr_t addr, in_port_t port);

    size_t size() const;
    bool empty() const;
    bool contains(in_addr_t addr, in_port_t port) const;

    in_addr_t getIp() const;
    in_port_t getPort() const;

   private:
    void runLoop(std::stop_token stok);

   private:
    std::jthread jt_;
    mutable std::mutex mut_;
    containers::Socket sfd_;
    boost::unordered_set<std::pair<in_addr_t, in_port_t>> users_;
};

#ifdef false
class TextChannel final : public Channel {
   public:
    TextChannel(size_t id) : Channel(id) {}

    void write(const std::string& data);
    void read(std::string& data);
};
#endif

}  // namespace server
