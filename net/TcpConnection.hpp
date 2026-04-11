#pragma once
#include <any>
#include <memory>
#include <atomic>
#include <chrono>
#include <functional>
#include "InetAddress.hpp"

namespace hyperMuduo::net {
    class EventLoop;
    class Channel;
    class Buffer;
    class Socket;

    class TcpConnection : public std::enable_shared_from_this<TcpConnection> {
    public:
        using ConnectionCallback = std::function<void(const std::shared_ptr<TcpConnection>&)>;
        using MessageCallback = std::function<void(const std::shared_ptr<TcpConnection>&,
                                                   Buffer&,
                                                   std::chrono::system_clock::time_point)>;
        using WriteCompleteCallback = std::function<void(const std::shared_ptr<TcpConnection>&)>;
        using HighWaterCallback = std::function<void(const std::shared_ptr<TcpConnection>&)>;
        using CloseCallback = std::function<void(const std::shared_ptr<TcpConnection>&)>;

        TcpConnection(EventLoop& loop,
                      std::string_view name,
                      Socket socket,
                      InetAddress local_addr,
                      InetAddress peer_addr);

        TcpConnection(const TcpConnection&) = delete;

        TcpConnection(TcpConnection&&) = delete;

        TcpConnection& operator=(const TcpConnection&) = delete;

        TcpConnection& operator=(TcpConnection&&) = delete;

        std::string getName() {
            return name_;
        }

        void shutdown();

        void setTcpNoDelay(bool on = true);

        void setKeepAlive(bool on = true);

        void send(Buffer& buffer);

        void send(std::string msg);

        void sendInLoop(const std::string& msg);

        void sendInLoop(const char* msg, size_t size);

        void sendInLoop(Buffer& buffer);


        bool connected() const {
            return state_ == State::Connected;
        }

        void shutdownInLoop();

        Buffer& sendBuffer();

        Buffer& receiveBuffer();

        EventLoop& getLoop() {
            return loop_;
        }

        void setMessageCallback(MessageCallback cb) {
            message_callback_ = std::move(cb);
        }

        void setConnectionCallback(ConnectionCallback cb) {
            connection_callback_ = std::move(cb);
        }

        void setWriteCompleteCallback(WriteCompleteCallback cb) {
            write_complete_callback_ = std::move(cb);
        }

        void setHighWaterCallback(HighWaterCallback cb, size_t high_water_mark) {
            high_water_mark_ = high_water_mark;
            high_water_callback_ = std::move(cb);
        }

        void setCloseCallback(CloseCallback cb) {
            close_callback_ = std::move(cb);
        }

        void connectionEstablished();

        void connectionDestroyed();

        template<typename T>
        bool hasContext() const {
            return context_.has_value() && context_.type() == typeid(T);
        }

        template<typename T>
        const T& getContextAs() const {
            return std::any_cast<T&>(context_);
        }

        template<typename T>
        T& getContextAs() {
            return std::any_cast<T&>(context_);
        }

        void setContext(std::any context);

    private:
        std::string getState() const;

        enum class State {
            Connecting,
            Connected,
            Disconnected,
            Disconnecting
        };

        void setState(State state) {
            state_ = state;
        }

        void handleReadable(std::chrono::system_clock::time_point time);

        void handleWritable();

        void handleClose();

        void handleError();

        EventLoop& loop_;
        std::string name_;
        std::atomic<State> state_;
        std::unique_ptr<Socket> socket_;
        std::unique_ptr<Channel> channel_;
        InetAddress local_address_;
        InetAddress peer_address_;
        std::unique_ptr<Buffer> receive_buffer_;
        std::unique_ptr<Buffer> send_buffer_;
        MessageCallback message_callback_;
        ConnectionCallback connection_callback_;
        size_t high_water_mark_;
        HighWaterCallback high_water_callback_;
        WriteCompleteCallback write_complete_callback_;
        CloseCallback close_callback_;

        std::any context_;

    };

    using TcpConnectionPtr = std::shared_ptr<TcpConnection>;
} // namespace hyper::net
