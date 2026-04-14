#include "TcpServer.hpp"

#include <spdlog/spdlog.h>

#include "Acceptor.hpp"
#include "EventLoop.hpp"
#include "EventLoopThread.hpp"
#include "EventLoopThreadPool.hpp"

hyperMuduo::net::TcpServer::TcpServer(EventLoop& loop, const InetAddress& listen_addr, std::string_view name)
    : loop_(loop),
      server_name_(name),
      acceptor_(std::make_unique<Acceptor>(loop, listen_addr)),
      high_water_mark_(64 * 1024 * 1024),
      started_(false),
      next_conn_id_(1),thread_pool_(std::make_unique<EventLoopThreadPool>(loop)) {
    acceptor_->setNewConnectionCallback([this](Socket socket, const InetAddress& peer_addr) {
        loop_.assertInLoopThread();
        char buf[32];
        std::snprintf(buf, sizeof(buf), "#%d", next_conn_id_++);
        std::string connection_name(server_name_ + buf);
        SPDLOG_INFO("TcpServer::NewConnection[{}] - new connection [{}] from {}", server_name_, connection_name,
                    peer_addr.toIpPort());
        InetAddress local_addr(socket.getLocalAddress());
        auto conn = std::make_shared<TcpConnection>(thread_pool_->getNextLoop(), connection_name, std::move(socket), local_addr, peer_addr);

        conn->setMessageCallback(message_callback_);
        conn->setConnectionCallback(connection_callback_);
        if (write_complete_callback_) {
            conn->setWriteCompleteCallback(write_complete_callback_);
        }
        conn->setCloseCallback([this, conn](const TcpConnectionPtr&) {
            removeConnection(conn);
        });
        if (high_watermark_callback_) {
            conn->setHighWaterCallback(high_watermark_callback_, high_water_mark_);
        }
        connections_[connection_name] = conn;
        
        // 关键：将 connectionEstablished 投递到 conn 所属的线程（子线程）执行
        conn->getLoop().runInLoop([conn]() {
            conn->connectionEstablished();
        });

    });
}

void hyperMuduo::net::TcpServer::start() {
    if (!started_) {
        started_ = true;
        loop_.runInLoop([this] {
            acceptor_->listen();
        });
        thread_pool_->start();
    }
}

void hyperMuduo::net::TcpServer::setThreadNum(int thread_num) {
    thread_pool_->setThreadNum(thread_num);
}

void hyperMuduo::net::TcpServer::removeConnection(const TcpConnectionPtr& conn) {
    loop_.runInLoop([this, conn]() {
        removeConnectionInLoop(conn);
    });
}

void hyperMuduo::net::TcpServer::removeConnectionInLoop(const TcpConnectionPtr& conn) {
    loop_.assertInLoopThread();
    SPDLOG_INFO("TcpServer::removeConnectionInLoop [{}] - connection {}", 
                server_name_, conn->getName());
    
    connections_.erase(conn->getName());
    
    // 关键：通知 TcpConnection 清理资源必须在 conn 所属的线程（子线程）执行
    conn->getLoop().queueInLoop([conn]() {
        conn->connectionDestroyed();
    });
}

hyperMuduo::net::TcpServer::~TcpServer() {
    started_ = false;
}
