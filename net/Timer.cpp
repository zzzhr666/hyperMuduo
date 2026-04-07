#include "Timer.hpp"

#include <spdlog/spdlog.h>

hyperMuduo::net::Timer::Timer(CallBack cb, TimePoint expiration_time, std::chrono::milliseconds interval)
    :expiration_time_(expiration_time),interval_(interval),cb_(std::move(cb)),sequence_(counter_.fetch_add(1)){}

void hyperMuduo::net::Timer::runCallback() {
    SPDLOG_TRACE("Executing callback.");
    cb_();
}

void hyperMuduo::net::Timer::restart() {
    if (isRepeated()) {
        SPDLOG_TRACE("Restarting.");
        expiration_time_ = std::chrono::steady_clock::now() + interval_;
    }
}
