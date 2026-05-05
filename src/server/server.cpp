#include <iostream>
#include <stdexcept>
#include <syncstream>
#include <utility>
#include <vector>

#include "server.hpp"

using namespace server;

Server::Server(size_t id) : id_{id} {}

void Server::run() {
    if (jt_.joinable())
        throw std::runtime_error("the server is already running!");
    jt_ = std::jthread([this](std::stop_token stok) { runLoop(stok); });
}

void Server::stop() {
    if (jt_.joinable()) {
        jt_.request_stop();
        jt_.join();
    }
}

bool Server::joinable() const noexcept {
    return jt_.joinable();
}

void Server::insert(const containers::Socket& sfd) {
    sfds_[sfd] = std::make_shared<DataUser>(sfd);
    epoll_event ev;
    ev.events = EPOLLIN | EPOLLOUT | EPOLLERR | EPOLLHUP | EPOLLRDHUP | EPOLLET;
    ev.data.fd = sfd;
    ep_.insert(sfd, ev);
}

void Server::erase(const containers::Socket& sfd) {
    if (contains(sfd)) {
        ep_.erase(sfd);
        sfds_.erase(sfd);
    }
}

size_t Server::size() const {
    return sfds_.size();
}

bool Server::empty() const {
    return sfds_.empty();
}

bool Server::contains(const containers::Socket& sfd) const {
    return sfds_.contains(sfd);
}

void Server::runLoop(std::stop_token stok) {
    try {
        epoll_event evs[Options::MAX_EVENTS];
        containers::Socket sfd;
        std::string data;
        std::shared_ptr<DataUser> du;

        containers::Parse p;
        ParseInfo pinfo{data, du};

        data.reserve(DATA_BYTES_MAX_LEN);
        fillParse(p, pinfo);

        auto handleInput = [&] {
            data = sfd.recv();
            if (data.length() >= 2) {
                uint16_t op;
                std::memcpy(&op, data.data(), 2);
                p.execute(op);
            } else
                throw std::out_of_range("server (loop): receive incorrect message from client (-1)");
        };

        auto handleOutput = [&] {
            if (*du != chs_.size()) {
                try {
                    std::vector<char> ids;
                    ids.reserve(2 + chs_.size() * sizeof(uint64_t));
                    ids.push_back(0);
                    ids.push_back(10);
                    for (auto& [first, second] : chs_) {
                        char* bytes = reinterpret_cast<char*>(&second);
                        ids.insert(ids.end(), bytes, bytes + sizeof(uint64_t));
                    }
                    sfd.send(ids.data(), ids.size(), 0);
                    *du = chs_.size();
                } catch (const std::runtime_error& re) {
                    throw;
                } catch (const std::exception& e) {
                    std::osyncstream(std::cerr) << e.what() << '\n';
                }
            }
            if (du->connectChannel) {
                in_port_t port = static_cast<VoiceChannel>(*chs_[du->channelIndex]).port();
                char* bytes = reinterpret_cast<char*>(port);
                char data[4] = {0, 11, bytes[0], bytes[1]};
                sfd.send(data, sizeof(data), 0);
                *du = chs_.size();
                du->connectChannel = false;
            }
        };

        while (!stok.stop_requested()) {
            int c = ep_.wait(evs, Options::MAX_EVENTS, Options::TIMEOUT_MS);
            for (int i = 0; i < c; ++i) {
                du = sfds_[evs[i].data.fd];
                sfd = static_cast<containers::Socket>(*du);
                try {
                    if (evs[i].events & (EPOLLIN | EPOLLRDHUP))
                        handleInput();
                    if (evs[i].events & EPOLLOUT)
                        handleOutput();
                    if (evs[i].events & (EPOLLERR | EPOLLHUP | EPOLLRDHUP))
                        erase(sfd);
                } catch (const std::out_of_range& oor) {
                    std::osyncstream(std::cerr) << oor.what() << '\n';
                } catch (const std::runtime_error& re) {
                    erase(sfd);
                    std::osyncstream(std::cerr) << re.what() << '\n';
                } catch (const std::exception& e) {
                    std::osyncstream(std::cout) << e.what() << '\n';
                }
            }
        }
    } catch (const std::exception& e) {
        std::osyncstream(std::cout) << e.what() << '\n';
    }
}

void Server::fillParse(containers::Parse& p, ParseInfo& pinfo) {
    p.insert(5, [&] {
        if (pinfo.data.length() != 17)
            throw std::out_of_range("server (loop): receive incorrect message from client (5)");
        uint64_t chId;
        in_addr_t ip;
        in_port_t port;
        char act;
        std::memcpy(&chId, pinfo.data.data() + 2, 8);
        std::memcpy(&ip, pinfo.data.data() + 10, 4);
        std::memcpy(&port, pinfo.data.data() + 14, 2);
        std::memcpy(&act, pinfo.data.data() + 16, 1);
        auto ch = static_cast<VoiceChannel>(*chs_[chId]);
        if (act) {
            ch.insert(ip, port);
            if (!ch.joinable())
                ch.run();
            pinfo.du->connectChannel = true;
            pinfo.du->channelIndex = chId;
        } else {
            ch.erase(ip, port);
            if (ch.empty())
                ch.stop();
        }
    });
    p.insert(7, [&] {
        chs_.insert(std::pair<size_t, std::unique_ptr<Channel>>{chIdx_, std::make_unique<VoiceChannel>(chIdx_)});
        ++chIdx_;
    });
    p.insert(8, [&] {
        if (pinfo.data.length() != 10)
            throw std::out_of_range("server (loop): receive incorrect message from client (8)");
        uint64_t chId;
        std::memcpy(&chId, pinfo.data.data() + 2, 8);
        chs_.erase(chId);
    });
}
