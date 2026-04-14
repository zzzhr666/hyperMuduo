#pragma once
#include <functional>
#include <atomic>
#include <chrono>
#include <memory>
#include "InetAddress.hpp"
#include "Socket.hpp"

namespace hyperMuduo::net {
    class EventLoop;
    class Socket;
    class Channel;

    class Connector:public std::enable_shared_from_this<Connector>{
    public:
        static constexpr auto MAX_RETRY_DELAY = std::chrono::seconds(30);
        static constexpr auto DEFAULT_RETRY_DELAY = std::chrono::milliseconds(500);

        using NewConnectionCallback = std::function<void(Socket)>;

        Connector(EventLoop& loop, const InetAddress& address);

        ~Connector();

        Connector(const Connector&) = delete;

        Connector& operator=(const Connector&) = delete;

        Connector(Connector&&) = delete;

        Connector& operator=(Connector&&) = delete;

        void setNewConnectionCallback(NewConnectionCallback callback) {
            new_connection_callback_ = std::move(callback);
        }

        void start();

        void restart();

        void stop();

    private:
        enum class State {
            Disconnected,
            Connecting,
            Connected,
        };

        void setState(const State state) {
            state_ = state;
        }
        void startInLoop();

        void stopInLoop();

        void connect();

        void connecting();

        void handleWritable();

        void resetChannel();

        void retry();


        static std::string getStateAsString(State state);


        EventLoop& loop_;
        InetAddress address_;
        std::atomic<bool> connect_;
        NewConnectionCallback new_connection_callback_;
        std::atomic<State> state_;
        std::unique_ptr<Socket> socket_;
        std::unique_ptr<Channel> channel_;
        std::chrono::milliseconds retry_delay_;
    };
}
