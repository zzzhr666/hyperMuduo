#include "net/EventLoop.hpp"
#include "net/TcpServer.hpp"
#include <thread>
#include <spdlog/spdlog.h>
#include <csignal>

int main(){
    using namespace hyperMuduo::net;
    
    SPDLOG_INFO("Server starting... This test will trigger SIGPIPE");
    SPDLOG_INFO("Connect with: nc localhost 9981, then close it immediately");
    
    EventLoop loop;
    TcpServer server(loop, InetAddress(9981), "test_SIGPIPE");
    
    server.setMessageCallback([](TcpConnectionPtr const& conn, auto& buffer, auto) {
        ::sleep(5);
        conn->send("First response");
        conn->send("Second response - this may trigger SIGPIPE");
    });
    
    server.setConnectionCallback([](TcpConnectionPtr const& conn) {
        if (conn->connected()) {
            SPDLOG_INFO("New connection from {}", conn->getName());
        } else {
            SPDLOG_INFO("Connection destroyed: {}", conn->getName());
        }
    });
    
    server.start();
    loop.loop();
}

