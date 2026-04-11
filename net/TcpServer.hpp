#pragma once
#include <chrono>
#include <functional>
#include <memory>
#include <unordered_map>



namespace hyperMuduo::net {
    class Socket;
    class InetAddress;
    class TcpConnection;
    using TcpConnectionPtr = std::shared_ptr<TcpConnection>;
    class EventLoop;
    class Buffer;
    class Acceptor;
    class EventLoopThreadPool;

    class TcpServer {
    public:
        using ConnectionCallback = std::function<void(const std::shared_ptr<TcpConnection>&)>;
        using MessageCallback = std::function<void(const std::shared_ptr<TcpConnection>&,
                                                   Buffer&,
                                                   std::chrono::system_clock::time_point)>;
        using HighWaterMarkCallback = std::function<void(const std::shared_ptr<TcpConnection>&)>;
        using WriteCompleteCallback = std::function<void(const std::shared_ptr<TcpConnection>&)>;

        TcpServer(EventLoop& loop, const InetAddress& listen_addr, std::string_view name);

        void start();

        void setMessageCallback(MessageCallback cb) {
            message_callback_ = std::move(cb);
        }

        void setConnectionCallback(ConnectionCallback cb) {
            connection_callback_ = std::move(cb);
        }

        void setWriteCompleteCallback(WriteCompleteCallback cb) {
            write_complete_callback_ = std::move(cb);
        }

        void setHighWaterMarkCallback(HighWaterMarkCallback cb,size_t high_water_mark) {
            high_watermark_callback_ = std::move(cb);
            high_water_mark_ = high_water_mark;
        }
        void setThreadNum(int thread_num);

        ~TcpServer();

    private:
        using ConnectionMap = std::unordered_map<std::string,std::shared_ptr<TcpConnection>>;

        void removeConnection(const TcpConnectionPtr& conn);
        void removeConnectionInLoop(const TcpConnectionPtr& conn);

    private:
        EventLoop& loop_;
        const std::string server_name_;
        std::unique_ptr<Acceptor> acceptor_;
        ConnectionCallback connection_callback_;
        MessageCallback message_callback_;
        WriteCompleteCallback write_complete_callback_;
        HighWaterMarkCallback high_watermark_callback_;
        size_t high_water_mark_;
        bool started_;
        int next_conn_id_;
        ConnectionMap connections_;
        std::unique_ptr<EventLoopThreadPool> thread_pool_;


    };
}
