#include <iostream>
#include <stdexcept>
#include <utility>

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
    ep_.erase(sfd);
    sfds_.erase(sfd);
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
        std::regex re(R"(\b[1-9]+[0-9]*\b)");
        containers::Socket sfd;
        std::string data;
        std::smatch match;
        containers::Parse p;
        ParseInfo pinfo{match, sfd};

        data.reserve(DATA_BYTES_MAX_LEN);
        fillParse(p, pinfo);

        while (!stok.stop_requested()) {
            int c = ep_.wait(evs, Options::MAX_EVENTS, Options::TIMEOUT_MS);
            for (int i = 0; i < c; ++i) {
                auto& dataUser = *sfds_[evs[i].data.fd];
                sfd = static_cast<containers::Socket>(dataUser);
                if (evs[i].events & (EPOLLIN | EPOLLRDHUP)) {
                    try {
                        data = sfd.recv();
                        if (std::regex_search(data, match, re))
                            p.execute(std::stoul(match[0]));
                    } catch (const std::exception& e) {
                        std::cerr << e.what();
                    }
                    data.clear();
                }
                if (evs[i].events & EPOLLOUT) {
                    if (dataUser != chs_.size())
                        try {
                            sfd.send("10 " + getChannelsIds());
                            dataUser = chs_.size();
                        } catch (const std::exception& e) {
                            std::cerr << e.what();
                        }
                }
                if (evs[i].events & (EPOLLERR | EPOLLHUP | EPOLLRDHUP)) {
                    ep_.erase(sfd);
                    sfds_.erase(sfd);
                    sfd.close();
                    continue;
                }
            }
        }
    } catch (const std::exception& e) {
        std::cout << e.what();
    }
}

void Server::fillParse(containers::Parse& p, ParseInfo& pinfo) {
    // p.insert(3, [&] {});
    p.insert(5, [&] {
        if (pinfo.match[1].str().empty())
            throw std::runtime_error("server (loop): invalid message from client!");
        auto ch = static_cast<VoiceChannel>(*chs_[std::stoul(pinfo.match[1])]);
        ch.insert(pinfo.sfd.ip(), pinfo.sfd.port());
        if (!ch.joinable())
            ch.run();
    });
    p.insert(6, [&] {
        auto ch = static_cast<VoiceChannel>(*chs_[std::stoul(pinfo.match[1])]);
        ch.erase(pinfo.sfd.ip(), pinfo.sfd.port());
        if (ch.empty())
            ch.stop();
    });
    p.insert(7, [&] {
        chs_.insert(std::pair<size_t, std::unique_ptr<Channel>>{chIdx_, std::make_unique<VoiceChannel>(chIdx_)});
        ++chIdx_;
    });
    p.insert(8, [&] { chs_.erase(std::stoul(pinfo.match[1])); });
}

std::string Server::getChannelsIds() const {
    std::string ids;
    for (auto& [first, second] : chs_) {
        ids += *second + ' ';
    }
    return ids;
}
