#include <thread>
#include <chrono>

#include "net/EventLoop.hpp"


int main() {
    hyperMuduo::net::EventLoop loop;
    std::thread t([&loop]() {
        loop.runAfter(std::chrono::seconds(1),[](){});
    });
}
