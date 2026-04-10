#include "Acceptor.hpp"

#include "EventLoop.hpp"

hyperMuduo::net::Acceptor::Acceptor(EventLoop& loop, const InetAddress& listen_address)
    : loop_(loop), accept_channel_(loop, accept_socket_.getFd()), is_listening_(false) {
    accept_socket_.setReuseAddr();
    accept_socket_.bindAddress(listen_address);
    accept_channel_.setReadCallback([this](std::chrono::system_clock::time_point time) {
        loop_.assertInLoopThread();
        InetAddress peer_addr{0};
        Socket conn_socket = accept_socket_.accept(peer_addr);
        if (conn_socket.valid()) {
            new_connection_callback_(std::move(conn_socket),peer_addr);
        }
    });
}

void hyperMuduo::net::Acceptor::listen() {
    loop_.assertInLoopThread();
    is_listening_ = true;
    accept_socket_.listen();
    accept_channel_.listenTillReadable();
}
