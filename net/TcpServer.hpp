#pragma once
#include <chrono>
#include <functional>
#include <memory>
#include <unordered_map>


namespace hyperMuduo::net {
    class Socket;
    class InetAddress;
    class TcpConnection;
    class EventLoop;
    class Buffer;
    class Acceptor;

    class TcpServer {
    public:
        using ConnectionCallback = std::function<void(const std::shared_ptr<TcpConnection>&)>;
        using MessageCallback = std::function<void(const std::shared_ptr<TcpConnection>&,
                                                   Buffer&,
                                                   std::chrono::system_clock::time_point)>;

        TcpServer(EventLoop& loop, const InetAddress& listen_addr, std::string_view name);

        void start();

        void setMessageCallback(MessageCallback cb) {
            message_callback_ = std::move(cb);
        }

        void setConnectionCallback(ConnectionCallback cb) {
            connection_callback_ = std::move(cb);
        }

        ~TcpServer();

    private:
        using ConnectionMap = std::unordered_map<std::string,std::shared_ptr<TcpConnection>>;

    private:
        EventLoop& loop_;
        const std::string server_name_;
        std::unique_ptr<Acceptor> acceptor_;
        ConnectionCallback connection_callback_;
        MessageCallback message_callback_;
        bool started_;
        int next_conn_id_;
        ConnectionMap connections_;


    };
}
