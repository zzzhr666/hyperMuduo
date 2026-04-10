#pragma once
#include <functional>
#include <sys/poll.h>
#include <chrono>
#include <memory>

namespace hyperMuduo::net {
    class EventLoop;
    class TcpConnection;

    class Channel {
    public:
        using EventCallback = std::function<void()>;
        using ReadEventCallback = std::function<void(std::chrono::system_clock::time_point)>;

        static constexpr int NOT_FILL = -1;

        Channel(const Channel&) = delete;

        Channel& operator=(const Channel&) = delete;

        Channel(Channel&&) = delete;

        Channel& operator=(Channel&&) = delete;

        Channel(EventLoop& loop, int fd);

        void handleEvent(std::chrono::system_clock::time_point tp);

        void handleEventWithGuard();

        void tie(const std::shared_ptr<TcpConnection>& conn);

        void setReadCallback(ReadEventCallback cb) {
            read_callback_ = std::move(cb);
        }

        void setWriteCallback(EventCallback cb) {
            write_callback_ = std::move(cb);
        }

        void setErrorCallback(EventCallback cb) {
            error_callback_ = std::move(cb);
        }

        [[nodiscard]] int getFd() const {
            return fd_;
        }

        [[nodiscard]] int getEvents() const {
            return events_;
        }

        [[nodiscard]] int getReturnEvents() const {
            return return_events_;
        }

        void setReturnEvents(int return_events) {
            return_events_ = return_events;
        }

        [[nodiscard]] bool isNoneEvent() const {
            return events_ == kNoneEvent;
        }

        void listenTillReadable() {
            events_ |= kReadEvent;
            notifyLoop();
        }

        void listenTillWritable() {
            events_ |= kWriteEvent;
            notifyLoop();
        }



        void ignoreReadable() {
            events_ &= ~kReadEvent;
            notifyLoop();
        }

        void ignoreWritable() {
            events_ &= ~kWriteEvent;
            notifyLoop();
        }

        void ignoreAll() {
            events_ = kNoneEvent;
            notifyLoop();
        }

        bool isWriting() const {
            return events_ & kWriteEvent;
        }

        EventLoop& getOwnerLoop() const {
            return loop_;
        }

        [[nodiscard]] int index() const {
            return index_;
        }

        void setIndex(int index) {
            index_ = index;
        }

        [[nodiscard]] EventLoop& loop() const {
            return loop_;
        }

    private:
        void notifyLoop();

    private:
        static constexpr int kNoneEvent = 0;
        static constexpr int kReadEvent = POLLIN | POLLPRI;
        static constexpr int kWriteEvent = POLLOUT;
        EventLoop& loop_;
        const int fd_;
        int events_;
        int return_events_;

        int index_; //used by poller

        std::weak_ptr<TcpConnection> parent_;
        bool tied_;

        ReadEventCallback read_callback_;
        EventCallback write_callback_;
        EventCallback error_callback_;
        EventCallback close_callback_;
    };
}
