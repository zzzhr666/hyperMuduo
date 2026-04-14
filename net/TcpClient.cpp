#include "TcpClient.hpp"

#include <spdlog/spdlog.h>

#include "Connector.hpp"
#include "EventLoop.hpp"

hyperMuduo::net::TcpClient::TcpClient(EventLoop& loop, const InetAddress& server_addr, std::string_view name)
    : loop_(loop),
      name_(name),
      address_(server_addr),
      connector_(std::make_shared<Connector>(loop, address_)),
      high_water_mark_(64 * 1024 * 1024),
      next_conn_id_(0),
      retry_(false),
connect_(false){
    connector_->setNewConnectionCallback([this](Socket socket) {
        onConnection(std::move(socket));
    });
}

void hyperMuduo::net::TcpClient::connect() {
    connect_ = true;
    connector_->start();
}

void hyperMuduo::net::TcpClient::disconnect() {
    connect_ = false;
    std::lock_guard<std::mutex> lock(mutex_);
    if (connection_) {
        connection_->shutdown();
    }
}

void hyperMuduo::net::TcpClient::stop() {
    connect_ = false;
    connector_->stop();
}


hyperMuduo::net::TcpClient::~TcpClient() {
    SPDLOG_DEBUG("TcpClient::~TcpClient[{}] - connector {}", name_, fmt::ptr(connector_.get()));
    TcpConnectionPtr conn;

    //lock scope
    {
        std::lock_guard<std::mutex> lock(mutex_);
        conn = connection_;
    }

    if (conn) {
        auto& loop = conn->getLoop();
        loop.runInLoop([conn]() {
            conn->setCloseCallback([](const TcpConnectionPtr&) {
                SPDLOG_DEBUG("TcpClient connection closed after client destruction");

            });
            conn->connectionDestroyed();
        });
    } else {
        connector_->stop();
    }
}

void hyperMuduo::net::TcpClient::onConnection(Socket socket) {
    loop_.assertInLoopThread();
    std::string name = name_ + "#" + std::to_string(next_conn_id_.fetch_add(1));
    auto local_addr = socket.getLocalAddress();
    auto peer_addr = socket.getPeerAddress();
    SPDLOG_DEBUG("TcpClient::onConnection[{}] connected to {}", name, local_addr.toIpPort());
    auto conn = std::make_shared<TcpConnection>(loop_, std::move(name), std::move(socket), local_addr, peer_addr);
    if (connection_callback_) {
        conn->setConnectionCallback(connection_callback_);
    }
    if (message_callback_) {
        conn->setMessageCallback(message_callback_);
    }
    if (high_water_mark_callback_) {
        conn->setHighWaterCallback(high_water_mark_callback_, high_water_mark_);
    }
    if (write_complete_callback_) {
        conn->setWriteCompleteCallback(write_complete_callback_);
    }

    conn->setCloseCallback([this](const TcpConnectionPtr& conn) {
        //lock scope
        {
            std::lock_guard<std::mutex> lock(mutex_);
            connection_.reset();
        }
        conn->getLoop().queueInLoop([conn]() {
            conn->connectionDestroyed();
        });
        if (retry_ && connect_) {
            connector_->restart();
        }

    });    //lock scope
    {
        std::lock_guard<std::mutex> lock(mutex_);
        connection_ = conn;
    }
    conn->connectionEstablished();

}
