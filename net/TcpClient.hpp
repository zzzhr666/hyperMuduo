#pragma once

#include <chrono>
#include <functional>
#include <memory>
#include <string>
#include "InetAddress.hpp"
#include "TcpConnection.hpp"

namespace hyperMuduo::net {
    class EventLoop;
    class TcpConnection;
    class Connector;
    class Buffer;

    class TcpClient {
    public:
        using ConnectionCallback = std::function<void(const std::shared_ptr<TcpConnection>&)>;
        using WriteCompleteCallback = std::function<void(const std::shared_ptr<TcpConnection>&)>;
        using MessageCallback = std::function<void(const std::shared_ptr<TcpConnection>&,
                                                   Buffer&,
                                                   std::chrono::system_clock::time_point)>;
        using HighWaterMarkCallback = std::function<void(const std::shared_ptr<TcpConnection>&)>;
        TcpClient(EventLoop& loop, const InetAddress& server_addr, std::string_view name);

        void setConnectionCallback(ConnectionCallback cb) {
            connection_callback_ = std::move(cb);
        }

        void setMessageCallback(MessageCallback cb) {
            message_callback_ = std::move(cb);
        }


        void setWriteCompleteCallback( WriteCompleteCallback cb) {
            write_complete_callback_ = std::move(cb);
        }

        void setHighWaterMarkCallback(HighWaterMarkCallback cb,size_t high_water_mark) {
            high_water_mark_ = high_water_mark;
            high_water_mark_callback_ = std::move(cb);
        }

        void connect();


        void disconnect();

        void stop();

        EventLoop& getLoop() const {
            return loop_;
        }

        std::shared_ptr<TcpConnection> getConnection() const {
            return connection_;
        }

        std::atomic<bool> getRetryFlag() const {
            return retry_.load(std::memory_order_relaxed);
        }

        const std::string& getName() const {
            return name_;
        }


        void setRetry(bool on = true) {
            retry_ = on;
        }

        ~TcpClient();

    private:
        void onConnection(Socket socket);


    private:
        EventLoop& loop_;
        std::string name_;
        InetAddress address_;
        std::shared_ptr<Connector> connector_;
        ConnectionCallback connection_callback_;
        MessageCallback message_callback_;
        WriteCompleteCallback write_complete_callback_;
        HighWaterMarkCallback high_water_mark_callback_;
        size_t high_water_mark_;
        std::mutex mutex_;
        std::shared_ptr<TcpConnection> connection_;
        std::atomic<int> next_conn_id_;
        std::atomic<bool> retry_;
        std::atomic<bool> connect_;

    };

}
