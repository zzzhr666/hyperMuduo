#include "Socket.hpp"

#include <system_error>
#include <unistd.h>
#include <spdlog/spdlog.h>
#include <sys/socket.h>
#include <netinet/tcp.h>

hyperMuduo::net::Socket::Socket()
    : socket_fd_(socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC,IPPROTO_TCP)) {
    if (socket_fd_ == -1) {
        SPDLOG_CRITICAL("Unable to create new Socket,errno msg:{}", std::system_category().message(errno));
        std::abort();
    }
}

hyperMuduo::net::Socket::Socket(int sock_fd)
    : socket_fd_(sock_fd) {
}

hyperMuduo::net::Socket::Socket(Socket&& other) noexcept
    : socket_fd_(other.getFd()) {
    other.invalidate();
}

hyperMuduo::net::Socket& hyperMuduo::net::Socket::operator=(Socket&& other) noexcept {
    if (this != &other) {
        if (socket_fd_ != INVALID_FD) {
            close(socket_fd_);
        }
        socket_fd_ = other.getFd();
        other.invalidate();
    }
    return *this;
}

void hyperMuduo::net::Socket::invalidate() {
    socket_fd_ = INVALID_FD;
}

hyperMuduo::net::Socket hyperMuduo::net::Socket::accept(InetAddress& addr) {
    sockaddr_in* addr_in = addr.getSockAddrInMutable();
    socklen_t length = sizeof(sockaddr_in);
    int conn_fd = accept4(socket_fd_, reinterpret_cast<sockaddr*>(addr_in), &length, SOCK_NONBLOCK | SOCK_CLOEXEC);
    if (conn_fd >= 0) {
        return Socket(conn_fd);
    }

    if (errno != EAGAIN && errno != EWOULDBLOCK) {
        SPDLOG_ERROR("socket accept error: {}", std::system_category().message(errno));
    }
    return Socket{INVALID_FD};
}

void hyperMuduo::net::Socket::setTcpNoDelay(bool on) {
    int optval = on ? 1 : 0;
    if (::setsockopt(socket_fd_,IPPROTO_TCP,TCP_NODELAY,&optval,sizeof(optval)) < 0) {
        SPDLOG_ERROR("Failed to set TCP_NODELAY ,error message: {}", std::system_category().message(errno));
    }
}

void hyperMuduo::net::Socket::setKeepAlive(bool on) {
    int optval = on ? 1 : 0;
    if (::setsockopt(socket_fd_,SOL_SOCKET,SO_KEEPALIVE,&optval,sizeof(optval))<0) {
        SPDLOG_ERROR("Failed to set SO_KEEPALIVE,error message: {}", std::system_category().message(errno));
    }
}

void hyperMuduo::net::Socket::bindAddress(const InetAddress& addr) {
    int ret = ::bind(socket_fd_, reinterpret_cast<const sockaddr*>(addr.getSockAddrIn()), sizeof(sockaddr_in));
    if (ret == -1) {
        SPDLOG_CRITICAL("Failed to bind with {},errno msg:{}", addr.toIpPort(), std::system_category().message(errno));
        std::abort();
    }
}

void hyperMuduo::net::Socket::listen() {
    int ret = ::listen(socket_fd_,SOMAXCONN);
    if (ret == -1) {
        SPDLOG_CRITICAL("Listen failed ,errno msg:{} ", std::system_category().message(errno));
        std::abort();
    }
}

void hyperMuduo::net::Socket::setReuseAddr() {
    int optval = 1;
    ::setsockopt(socket_fd_,SOL_SOCKET,SO_REUSEADDR, &optval, sizeof(int));
}

void hyperMuduo::net::Socket::setReusePort() {
    int optval = 1;
    ::setsockopt(socket_fd_,SOL_SOCKET,SO_REUSEPORT, &optval, sizeof(int));
}

void hyperMuduo::net::Socket::setKeepAlive() {
    int optval = 1;
    ::setsockopt(socket_fd_,SOL_SOCKET,SO_KEEPALIVE, &optval, sizeof(int));
}

hyperMuduo::net::InetAddress hyperMuduo::net::Socket::getLocalAddress() const {
    sockaddr_in local;
    socklen_t length = sizeof(local);
    std::memset(&local, 0, sizeof(local));
    int ret = getsockname(socket_fd_, reinterpret_cast<sockaddr*>(&local), &length);\
    if (ret == -1) {
        SPDLOG_ERROR("Failed to getsockname,msg:{}", std::system_category().message(errno));

    }
    return InetAddress(local);
}

hyperMuduo::net::InetAddress hyperMuduo::net::Socket::getPeerAddress() const {
    sockaddr_in peer;
    socklen_t length = sizeof(peer);
    int ret = getpeername(socket_fd_, reinterpret_cast<sockaddr*>(&peer), &length);
    if (ret == -1) {
        SPDLOG_ERROR("Failed to getpeername,msg:{}", std::system_category().message(errno));
    }
    return InetAddress(peer);
}

int hyperMuduo::net::Socket::getSocketError() const {
    int optval = 1;
    socklen_t length = sizeof(optval);

    if (::getsockopt(socket_fd_, SOL_SOCKET, SO_ERROR, &optval, &length) < 0) {
        return errno;
    } else {
        return optval;
    }
}

void hyperMuduo::net::Socket::shutdownWrite() {
    if (::shutdown(socket_fd_, SHUT_RDWR) == -1) {
        SPDLOG_ERROR("Socket::shutdownWrite error:{}", std::system_category().message(errno));
    }
}


hyperMuduo::net::Socket::~Socket() {
    if (socket_fd_ != INVALID_FD) {
        close(socket_fd_);
    }
}
