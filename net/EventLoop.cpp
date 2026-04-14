#include "EventLoop.hpp"

#include <cassert>
#include <cstdlib>
#include <spdlog/spdlog.h>
#include <sys/eventfd.h>
#include <sys/poll.h>
#include <sys/signal.h>

#include "Channel.hpp"
#include "Poller.hpp"
#include "TimerQueue.hpp"
#if defined(__linux__)
#include "Epoller.hpp"
#endif


namespace {
    thread_local hyperMuduo::net::EventLoop* t_current_thread_loop;

    int create_eventfd() {
        int event_fd = eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
        if (event_fd == -1) {
            SPDLOG_ERROR("Failed to create event fd");
            std::abort();
        }
        return event_fd;
    }

    class IgnoreSigPipe {
    public:
        IgnoreSigPipe() {
            struct sigaction sa;
            sa.sa_handler = SIG_IGN;
            sigemptyset(&sa.sa_mask);
            sa.sa_flags = 0;
            ::sigaction(SIGPIPE, &sa, nullptr);

        }
    };

    IgnoreSigPipe ignore_sigpipe;


    // =========================================================================
    // Poller 选择策略
    // =========================================================================
    // 默认行为：
    //   - Linux 平台：使用 Epoller (epoll)
    //   - 其他平台：使用 Poller (poll)
    //
    // 切换方法：
    //   在 Linux 上强制使用 poll：
    //     export HYPERMUDUO_USE_POLL=1
    //     ./your_application
    //
    //   恢复使用 epoll：
    //     unset HYPERMUDUO_USE_POLL
    //     ./your_application
    //
    //   或者单次运行指定：
    //     HYPERMUDUO_USE_POLL=1 ./your_application
    // =========================================================================

    std::unique_ptr<hyperMuduo::net::PollerBase> getPoller(hyperMuduo::net::EventLoop& loop) {
#if defined(__linux__)
        if (const char* env = std::getenv("HYPERMUDUO_USE_POLL"); env && std::string(env) == "1") {
            SPDLOG_INFO("Using poll poller(forced by env).");
            return std::make_unique<hyperMuduo::net::Poller>(loop);
        }
        SPDLOG_DEBUG("Using epoll poller (default on Linux)");
        return std::make_unique<hyperMuduo::net::Epoller>(loop);
#else
        SPDLOG_INFO("Using poll poller(default on platform other than Linux)");
        return std::make_unique<hyperMuduo::net::Poller>(loop);
#endif

    }
}


hyperMuduo::net::EventLoop::EventLoop()
    : is_running_{false},
      quit_{true},
      is_processing_tasks_(false),
      thread_id_{CurrentThread::getTid()},
      poller_(getPoller(*this)),
      timer_queue_(std::make_unique<TimerQueue>(*this)),
      wakeup_fd_(create_eventfd()),
      wakeup_channel_(std::make_unique<Channel>(*this, wakeup_fd_)) {
    SPDLOG_DEBUG("Eventloop created {} in thread {}", fmt::ptr(this), thread_id_);
    if (t_current_thread_loop == nullptr) {
        t_current_thread_loop = this;
    } else {
        SPDLOG_CRITICAL("Another EventLoop {} exists in this thread {}", fmt::ptr(t_current_thread_loop), thread_id_);
        std::abort();
    }
    wakeup_channel_->setReadCallback([this](std::chrono::system_clock::time_point when) {
        uint64_t n;
        ssize_t nr = ::read(wakeup_fd_, &n, sizeof(n));
        if (nr != sizeof(n)) {
            SPDLOG_ERROR("EventLoop::handleWakeupRead() reads {} bytes instead of 8", nr);
        }
    });
    wakeup_channel_->listenTillReadable();
}

hyperMuduo::net::EventLoop::~EventLoop() {
    assert(!is_running_);
    t_current_thread_loop = nullptr;
    wakeup_channel_->ignoreAll();
    ::close(wakeup_fd_);
}

void hyperMuduo::net::EventLoop::loop() {
    if (!is_running_) {
        assertInLoopThread();
        is_running_ = true;
        quit_ = false;
        while (!quit_) {
            ready_channels_.clear();
            poller_->poll(kPollTime, ready_channels_);
            for (auto* channel: ready_channels_) {
                channel->handleEvent(std::chrono::system_clock::now());
            }
            executePendingTasks();
        }
    }
    SPDLOG_TRACE("EventLoop {} stop looping.", fmt::ptr(this));
    is_running_ = false;
}

void hyperMuduo::net::EventLoop::quit() {
    quit_ = true;
    if (!isInLoopThread()) {
        wakeup();
    }
}

hyperMuduo::net::TimerId hyperMuduo::net::EventLoop::runAt(std::chrono::steady_clock::time_point tp,
                                                           std::function<void()> callback) {
    return timer_queue_->addTimer(std::move(callback), tp, {});
}

hyperMuduo::net::TimerId hyperMuduo::net::EventLoop::runEvery(std::chrono::milliseconds us,
                                                              std::function<void()> callback) {
    return timer_queue_->addTimer(std::move(callback), std::chrono::steady_clock::now() + us, us);
}


hyperMuduo::net::TimerId hyperMuduo::net::EventLoop::runAfter(std::chrono::milliseconds us,
                                                              std::function<void()> callback) {
    return timer_queue_->addTimer(std::move(callback), std::chrono::steady_clock::now() + us, {});
}

void hyperMuduo::net::EventLoop::cancelTimer(TimerId timerId) {
    timer_queue_->cancelTimer(timerId);
}


hyperMuduo::net::EventLoop* hyperMuduo::net::EventLoop::getEventLoopOfCurrentThread() {
    return t_current_thread_loop;
}

void hyperMuduo::net::EventLoop::assertInLoopThread() {
    if (!isInLoopThread()) {
        abortNotInLoopThread();
    }
}

void hyperMuduo::net::EventLoop::updateChannel(Channel& channel) {
    if (&channel.getOwnerLoop() == this) {
        assertInLoopThread();
        poller_->updateChannel(channel);
    }
}

void hyperMuduo::net::EventLoop::removeChannel(Channel& channel) {
    if (&channel.getOwnerLoop() == this) {
        assertInLoopThread();
        poller_->removeChannel(channel);
    }
}

void hyperMuduo::net::EventLoop::runInLoop(std::function<void()> callback) {
    if (isInLoopThread()) {
        callback();
    } else {
        queueInLoop(std::move(callback));
    }
}

void hyperMuduo::net::EventLoop::queueInLoop(std::function<void()> callback) {
    //lock scope
    {
        std::unique_lock<std::mutex> lock(task_mutex_);
        task_queue_.push_back(std::move(callback));
    }

    if (!isInLoopThread() || is_processing_tasks_) {
        wakeup();
    }
}

void hyperMuduo::net::EventLoop::abortNotInLoopThread() {
    SPDLOG_CRITICAL("EventLoop::abortNotInLoopThread - EventLoop {} was created in thread {},current thread id = {}",
                    fmt::ptr(this), thread_id_, CurrentThread::getTid());
    std::abort();
}

void hyperMuduo::net::EventLoop::wakeup() {
    uint64_t n{1};
    ssize_t nw = ::write(wakeup_fd_, &n, sizeof(n));
    if (nw != sizeof(n)) {
        SPDLOG_ERROR("EventLoop::wakeup() writes {} bytes instead of 8", nw);
    }
}

void hyperMuduo::net::EventLoop::executePendingTasks() {
    using Task = std::function<void()>;
    std::vector<Task> tasks;
    is_processing_tasks_ = true;
    //lock scope
    {
        std::lock_guard<std::mutex> lock(task_mutex_);
        tasks.swap(task_queue_);
    }


    for (auto& task: tasks) {
        task();
    }
    is_processing_tasks_ = false;

}
