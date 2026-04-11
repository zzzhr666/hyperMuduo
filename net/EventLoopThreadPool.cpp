#include "EventLoopThreadPool.hpp"

#include "EventLoopThread.hpp"

hyperMuduo::net::EventLoopThreadPool::EventLoopThreadPool(EventLoop& base_loop)
    : base_loop_(base_loop), started_(false), threads_num_(0), next_(0) {
}

void hyperMuduo::net::EventLoopThreadPool::start() {
    if (bool expect = false; !started_.compare_exchange_strong(expect, true)) {
        return;
    }
    if (threads_num_ > 0) {
        threads_.reserve(threads_num_);
        loops_.reserve(threads_num_);
    }

    for (int i = 0; i < threads_num_; ++i) {
        threads_.emplace_back(std::make_unique<EventLoopThread>("WorkerThread_" + std::to_string(i)));
        EventLoop& this_loop = threads_.back()->startLoop();
        loops_.emplace_back(&this_loop);
    }
}

hyperMuduo::net::EventLoop& hyperMuduo::net::EventLoopThreadPool::getNextLoop() {
    if (!loops_.empty()) {
        const int index = next_.fetch_add(1, std::memory_order_relaxed) % loops_.size();
        return *loops_[index];
    }
    return base_loop_;
}

hyperMuduo::net::EventLoopThreadPool::~EventLoopThreadPool() = default;
