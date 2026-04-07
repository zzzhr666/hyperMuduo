#pragma once

#include <chrono>
#include <utility>
#include <unordered_map>
#include <queue>

#include "Channel.hpp"
#include "Timer.hpp"

namespace hyperMuduo::net {
    class TimerId {
    public:
        explicit TimerId(int64_t sequence) : timer_id(sequence) {}

        [[nodiscard]] int64_t getTimerId() const {
            return timer_id;
        }

    private:
        int64_t timer_id;
    };

    class Timer;
    class EventLoop;

    class TimerQueue {
    public:
        explicit TimerQueue(EventLoop& loop);

        ~TimerQueue();

        TimerId addTimer(Timer::CallBack cb, Timer::TimePoint tp, std::chrono::milliseconds interval);


        void cancelTimer(TimerId timerId);

        void assertInLoopThread();

    private:
        using Sequence = int64_t; //TimerId
        using Entry = std::pair<std::chrono::steady_clock::time_point,Sequence>;
        using TimerPriQueue = std::priority_queue<Entry,std::vector<Entry>,std::greater<>>;
        using TimerMap = std::unordered_map<Sequence,std::unique_ptr<Timer>>;


        void handleExpiredTimers_();

        std::vector<Entry> getExpiredTimers_();

        void resetTimerFd_();



        EventLoop& loop_;
        const int timer_fd_;
        Channel timer_channel_;
        TimerPriQueue timer_queue_;
        TimerMap timers_;
    };

}
