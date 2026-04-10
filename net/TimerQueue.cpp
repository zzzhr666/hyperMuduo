#include "TimerQueue.hpp"

#include <ranges>
#include <unistd.h>
#include <spdlog/spdlog.h>
#include <sys/timerfd.h>
#include "EventLoop.hpp"

hyperMuduo::net::TimerQueue::TimerQueue(EventLoop& loop)
    : loop_(loop),
      timer_fd_(::timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK | TFD_CLOEXEC)),
      timer_channel_(loop, timer_fd_) {
    assertInLoopThread();
    timer_channel_.setReadCallback([this](std::chrono::system_clock::time_point) {
        handleExpiredTimers_();
    });
    timer_channel_.listenTillReadable();
}

hyperMuduo::net::TimerQueue::~TimerQueue() {
    loop_.removeChannel(timer_channel_);
    ::close(timer_fd_);
}

hyperMuduo::net::TimerId hyperMuduo::net::TimerQueue::addTimer(Timer::CallBack cb,
                                                               Timer::TimePoint tp,
                                                               std::chrono::milliseconds interval) {
    auto timer = std::make_unique<Timer>(std::move(cb),std::move(tp), std::move(interval));
    int64_t sequence = timer->getSequence();
    loop_.runInLoop([this,sequence,tp,t = timer.release()]()mutable {
        timers_[sequence] = std::unique_ptr<Timer>(t);
    timer_queue_.emplace(tp,sequence);
    if (tp == timer_queue_.top().first) {
        resetTimerFd_();
    }
    });
    return TimerId(sequence);
}

void hyperMuduo::net::TimerQueue::cancelTimer(TimerId timerId) {
    loop_.runInLoop([this,timerId]() {
        if (auto it = timers_.find(timerId.getTimerId()); it != timers_.end()) {
        timers_.erase(it);
    } else {
        SPDLOG_DEBUG("target TimerId = {} not found.", timerId.getTimerId());
    }
    });
}

void hyperMuduo::net::TimerQueue::assertInLoopThread() {
    loop_.assertInLoopThread();
}

void hyperMuduo::net::TimerQueue::handleExpiredTimers_() {
    uint64_t howmany;
    ssize_t nread = ::read(timer_fd_, &howmany, sizeof(howmany));
    if (nread != sizeof(howmany)) {
        SPDLOG_ERROR("Cannot read from timerfd");
        return;
    }
    auto expired_timers = getExpiredTimers_();
    for (auto& timer_sequence: expired_timers | std::views::values) {
        if (auto it = timers_.find(timer_sequence); it != timers_.end()) {
            auto& timer = it->second;
            timer->runCallback();
            if (timers_.contains(timer_sequence)) {
                if (timer->isRepeated()) {
                    timer->restart();
                    timer_queue_.emplace(timer->expiration(), timer_sequence);
                } else {
                    timers_.erase(timer_sequence);
                }
            }
        }
    }
    resetTimerFd_();
}

std::vector<hyperMuduo::net::TimerQueue::Entry> hyperMuduo::net::TimerQueue::getExpiredTimers_() {
    std::vector<Entry> expire_entries;
    auto now = std::chrono::steady_clock::now();
    while (!timer_queue_.empty() && timer_queue_.top().first <= now) {
        expire_entries.push_back(std::move(timer_queue_.top()));
        timer_queue_.pop();
    }
    return expire_entries;
}

void hyperMuduo::net::TimerQueue::resetTimerFd_() {
    itimerspec i_timer_spec{};
    if (!timer_queue_.empty()) {
        auto [earliest_tp,_] = timer_queue_.top();
        auto seconds = std::chrono::duration_cast<std::chrono::seconds>(earliest_tp.time_since_epoch());
        auto nanoseconds = std::chrono::duration_cast<std::chrono::nanoseconds>(
            earliest_tp.time_since_epoch() - seconds);
        i_timer_spec.it_value.tv_sec = seconds.count();
        i_timer_spec.it_value.tv_nsec = nanoseconds.count();
    }
    if (int ret = ::timerfd_settime(timer_fd_,TFD_TIMER_ABSTIME, &i_timer_spec, nullptr); ret == -1) {
        SPDLOG_ERROR("timerfd_settime failed:{}", strerror(errno));
    }
}
