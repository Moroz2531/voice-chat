#include "server.hpp"

#include <chrono>

using namespace std::chrono_literals;

int main() {
    server::Server sv{0};
    sv.run();
    std::this_thread::sleep_for(5s);
}