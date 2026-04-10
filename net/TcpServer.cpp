#include "TcpServer.hpp"

#include <spdlog/spdlog.h>

#include "Acceptor.hpp"
#include "EventLoop.hpp"

hyperMuduo::net::TcpServer::TcpServer(EventLoop& loop, const InetAddress& listen_addr, std::string_view name)
    : loop_(loop), server_name_(name),acceptor_(std::make_unique<Acceptor>(loop,listen_addr)), started_(false), next_conn_id_(1) {
    acceptor_->setNewConnectionCallback([this](Socket socket,const InetAddress& peer_addr){
        loop_.assertInLoopThread();
        char buf[32];
        std::snprintf(buf,sizeof(buf),"#%d",next_conn_id_++);
        std::string connection_name(server_name_ + buf);
        SPDLOG_INFO("TcpServer::NewConnection[{}] - new connection [{}] from {}",server_name_,connection_name,peer_addr.toIpPort());
        InetAddress local_addr(socket.getLocalAddress());
        auto conn = std::make_shared<TcpConnection>(loop_,connection_name,std::move(socket),local_addr,peer_addr);
        
        conn->setMessageCallback(message_callback_);
        conn->setConnectionCallback(connection_callback_);
        conn->setCloseCallback([this](const TcpConnectionPtr& conn) {
            loop_.assertInLoopThread();
            SPDLOG_INFO("TcpServer removeConnection [{}] - connection {}",server_name_,conn->getName());
            connections_.erase(conn->getName());
            loop_.queueInLoop([conn]() {
                conn->connectionDestroyed();
            });
        });
        connections_[connection_name] = conn;
        conn->connectionEstablished();

    });
}

void hyperMuduo::net::TcpServer::start() {
    if (!started_) {
        started_ = true;
        loop_.runInLoop([this] {
           acceptor_->listen();
        });
    }
}

hyperMuduo::net::TcpServer::~TcpServer() {
    started_ = false;
}
