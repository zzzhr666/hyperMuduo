#pragma once

#include <functional>
#include <chrono>
#include <atomic>


namespace hyperMuduo::net {
    class Timer {
    public:
        using CallBack = std::function<void()>;
        using TimePoint = std::chrono::steady_clock::time_point;
        Timer(CallBack cb,TimePoint expiration_time,std::chrono::milliseconds interval);

        TimePoint expiration() const {
            return expiration_time_;
        }

        bool isRepeated() const {
            return interval_.count() > 0;
        }

        void runCallback();
        void restart();

        [[nodiscard]]int64_t getSequence()const {
            return sequence_;
        }



    private:

        TimePoint expiration_time_;
        std::chrono::milliseconds interval_;
        CallBack cb_;
        int64_t sequence_;
        inline static std::atomic<int64_t> counter_{0};
    };
}
