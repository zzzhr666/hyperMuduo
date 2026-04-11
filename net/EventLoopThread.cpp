#include "EventLoopThread.hpp"

#include <csignal>
#include <spdlog/spdlog.h>

#include "EventLoop.hpp"



hyperMuduo::net::EventLoopThread::EventLoopThread( std::string_view thread_name ,ThreadInitCallback cb)
    : thread_name_(thread_name), loop_(nullptr), exiting_(false), cb_(std::move(cb)) {
}

hyperMuduo::net::EventLoopThread::~EventLoopThread() {
    exiting_ = true;
    if (loop_) {
        loop_->quit();
        if (thread_.joinable()) {
            thread_.join();
        }
    }
}

hyperMuduo::net::EventLoop& hyperMuduo::net::EventLoopThread::startLoop() {
    if (thread_.joinable()) {
        SPDLOG_CRITICAL("sub thread has been created!");
        std::abort();
    }
    thread_ = std::thread([this]() {
        EventLoop loop;
        if (cb_) {
            cb_(loop);
        }
        //lock scope
        std::unique_lock<std::mutex> lock(mutex_);
        loop_ = &loop; //NOLINT
        condition_.notify_one();
        lock.unlock();

        loop.loop();

        std::lock_guard<std::mutex> guard(mutex_);
        loop_ = nullptr;

    });


    std::unique_lock<std::mutex> lock(mutex_);
    condition_.wait(lock, [this]() {
        return loop_ != nullptr;
    });

    return *loop_;

}
