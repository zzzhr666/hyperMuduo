#pragma once
#include <atomic>
#include <chrono>
#include <functional>
#include <memory>
#include <unordered_map>
#include <vector>

#include "Base/CurrentThread.hpp"
#include "protobuf/Dispatcher.hpp"
#include "net/TimerQueue.hpp"

namespace hyperMuduo::net {
    class Channel;
    class Poller;


    class EventLoop {
    public:
        EventLoop();

        ~EventLoop();

        EventLoop(const EventLoop&) = delete;

        EventLoop& operator=(const EventLoop&) = delete;

        EventLoop(EventLoop&&) = delete;

        EventLoop& operator=(EventLoop&&) = delete;

        void loop();

        void quit();

        TimerId runAt(std::chrono::steady_clock::time_point tp,std::function<void()>callback);

        TimerId runEvery(std::chrono::milliseconds us,std::function<void()>callback);

        TimerId runAfter(std::chrono::milliseconds us,std::function<void()>callback);

        void cancelTimer(TimerId timerId);

        static EventLoop* getEventLoopOfCurrentThread();

        void assertInLoopThread();

        [[nodiscard]] bool isInLoopThread() const {
            return thread_id_ == CurrentThread::getTid();
        }

        void updateChannel(Channel& channel);

        void removeChannel(Channel& channel);

        void runInLoop(std::function<void()> callback);
        void queueInLoop(std::function<void()> callback);

    private:
        void abortNotInLoopThread();

    private:
        static constexpr std::chrono::milliseconds kPollTime{10};
        using ChannelList = std::vector<Channel*>;
        using ChannelMap = std::unordered_map<int, std::unique_ptr<Channel>>;

        void wakeup();

        void executePendingTasks();


        std::atomic<bool> is_running_;

        std::atomic<bool> quit_;

        std::atomic<bool> is_processing_tasks_;

        const int thread_id_;

        std::unique_ptr<Poller> poller_;


        ChannelList ready_channels_;

        std::unique_ptr<TimerQueue> timer_queue_;

        const int wakeup_fd_;

        std::unique_ptr<Channel> wakeup_channel_;

        std::mutex task_mutex_;

        std::vector<std::function<void()>> task_queue_;



    };
}
