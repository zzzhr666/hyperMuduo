#pragma once
#include <memory>
#include <vector>
#include <atomic>

namespace hyperMuduo::net {
    class EventLoop;
    class EventLoopThread;

    class EventLoopThreadPool {
    public:
        explicit EventLoopThreadPool(EventLoop& base_loop);

        EventLoopThreadPool(const EventLoopThreadPool&) = delete;

        EventLoopThreadPool& operator=(const EventLoopThreadPool&) = delete;

        EventLoopThreadPool(EventLoopThreadPool&&) = delete;

        EventLoopThreadPool& operator=(EventLoopThreadPool&&) = delete;

        void setThreadNum(int thread_num) {
            threads_num_ = thread_num;
        }

        void start();

        EventLoop& getNextLoop();

        ~EventLoopThreadPool();

    private:
        EventLoop& base_loop_;
        std::atomic<bool> started_;
        int threads_num_;
        std::atomic<int> next_;
        std::vector<std::unique_ptr<EventLoopThread>> threads_;
        std::vector<EventLoop*> loops_;
    };
}
