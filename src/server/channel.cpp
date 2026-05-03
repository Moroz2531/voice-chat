#include <netinet/in.h>
#include <cstring>
#include <iostream>
#include <stdexcept>

#include "channel.hpp"

using namespace server;

VoiceChannel::VoiceChannel(size_t id) : Channel(id) {
    sfd_.create(AF_INET, SOCK_DGRAM | SOCK_NONBLOCK, 0);
    sockaddr_in sin;
    std::memset(&sin, 0, sizeof(sockaddr_in));
    sin.sin_addr.s_addr = INADDR_ANY;
    sin.sin_family = AF_INET;
    sin.sin_port = 0;
    sfd_.bind(reinterpret_cast<sockaddr*>(&sin), sizeof(sockaddr_in));
}

void VoiceChannel::insert(in_addr_t addr, in_port_t port) {
    std::lock_guard<std::mutex> lock{mut_};
    users_.insert(std::pair<in_addr_t, in_port_t>{addr, port});
}

void VoiceChannel::erase(in_addr_t addr, in_port_t port) {
    std::lock_guard<std::mutex> lock{mut_};
    users_.erase(std::pair<in_addr_t, in_port_t>{addr, port});
}

in_addr_t VoiceChannel::getIp() const noexcept {
    return sfd_.ip();
}

in_port_t VoiceChannel::getPort() const noexcept {
    return sfd_.port();
}

void VoiceChannel::run() {
    if (jt_.joinable())
        throw std::runtime_error("the channel already is running");
    jt_ = std::jthread(&VoiceChannel::runLoop);
}

void VoiceChannel::stop() {
    if (jt_.joinable()) {
        jt_.request_stop();
        jt_.join();
    }
}

bool VoiceChannel::joinable() const noexcept {
    return jt_.joinable();
}

bool VoiceChannel::empty() const noexcept {
    return users_.empty();
}

size_t VoiceChannel::size() const noexcept {
    return users_.size();
}

bool VoiceChannel::contains(in_addr_t addr, in_port_t port) const {
    return users_.find(std::pair<in_addr_t, in_port_t>{addr, port}) != users_.end();
}

void VoiceChannel::runLoop(std::stop_token stok) noexcept {
    float data[DATA_FLOAT_LEN];
    sockaddr_in sinrecv, sinsend;
    socklen_t len;

    std::memset(&sinsend, 0, sizeof(sockaddr_in));
    sinsend.sin_family = AF_INET;
    try {
        while (!stok.stop_requested()) {
            len = sizeof(sockaddr_in);
            auto count = sfd_.recvfrom(data, DATA_FLOAT_LEN, 0, reinterpret_cast<sockaddr*>(&sinrecv), &len);
            if (!count || len != sizeof(sockaddr_in)) {
                std::this_thread::yield();
                continue;
            }
            std::pair<in_addr_t, in_port_t> u{sinrecv.sin_addr.s_addr, sinrecv.sin_port};
            auto usIt = users_.find(u);
            std::lock_guard<std::mutex> lock{mut_};
            if (usIt == users_.end())
                continue;
            for (auto it = users_.begin(); it != usIt; ++it) {
                sinsend.sin_addr.s_addr = it->first;
                sinsend.sin_port = it->second;
                sfd_.sendto(data, count, 0, reinterpret_cast<sockaddr*>(&sinsend), sizeof(sockaddr_in));
            }
            for (auto it = ++usIt, end = users_.end(); it != end; ++it) {
                sinsend.sin_addr.s_addr = it->first;
                sinsend.sin_port = it->second;
                sfd_.sendto(data, count, 0, reinterpret_cast<sockaddr*>(&sinsend), sizeof(sockaddr_in));
            }
        }
    } catch (const std::exception& e) {
        std::cout << "Error in VoiceChannel: " << e.what() << '\n';
    }
}
