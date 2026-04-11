#pragma once

#include <thread>
#include <mutex>
#include <condition_variable>
#include <functional>


namespace hyperMuduo::net {
    class EventLoop;

    class EventLoopThread {
    public:
        using ThreadInitCallback = std::function<void(EventLoop&)>;

        explicit EventLoopThread( std::string_view thread_name,ThreadInitCallback cb = {});

        ~EventLoopThread();

        EventLoopThread(const EventLoopThread&) = delete;

        EventLoopThread(EventLoopThread&&) = delete;

        EventLoopThread& operator=(const EventLoopThread&) = delete;

        EventLoopThread& operator=(EventLoopThread&&) = delete;

        EventLoop& startLoop();

    private:
        std::string thread_name_;
        EventLoop* loop_;
        bool exiting_;
        std::thread thread_;
        std::mutex mutex_;
        std::condition_variable condition_;
        ThreadInitCallback cb_;
    };
}
