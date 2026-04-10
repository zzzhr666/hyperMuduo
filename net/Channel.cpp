#include <spdlog/spdlog.h>
#include "Channel.hpp"

#include "EventLoop.hpp"

hyperMuduo::net::Channel::Channel(EventLoop& loop, int fd)
    : loop_{loop}, fd_{fd}, events_{kNoneEvent}, return_events_{0}, index_{NOT_FILL},tied_(false) {}

void hyperMuduo::net::Channel::handleEvent(std::chrono::system_clock::time_point tp) {
    if (return_events_ & POLLNVAL) {
        SPDLOG_WARN("Channel::handleEvent() POLLNVAL");
    }

    if (return_events_ & (POLLNVAL | POLLERR)) {
        if (error_callback_) {
            error_callback_();
        }
    }
    if (return_events_ & (POLLIN | POLLPRI | POLLRDHUP)) {
        if (read_callback_) {
            read_callback_(tp);
        }
    }

    if (return_events_ & POLLOUT) {
        if (write_callback_) {
            write_callback_();
        }
    }
}

void hyperMuduo::net::Channel::handleEventWithGuard() {
    if (!tied_) {
        SPDLOG_ERROR("No TcpConnection Tied");
        return;
    }
    if (auto guard = parent_.lock()) {
        handleEvent(std::chrono::system_clock::now());
    }
}



void hyperMuduo::net::Channel::tie(const std::shared_ptr<TcpConnection>& conn) {
    tied_ = true;
    parent_ = conn;
}



void hyperMuduo::net::Channel::notifyLoop() {
    loop_.updateChannel(*this);
}

