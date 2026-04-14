#include "Connector.hpp"

#include <spdlog/spdlog.h>

#include "Channel.hpp"
#include "EventLoop.hpp"
#include "Socket.hpp"

hyperMuduo::net::Connector::Connector(EventLoop& loop, const InetAddress& address)
    : loop_(loop), address_(address), connect_(false), state_(State::Disconnected), retry_delay_(DEFAULT_RETRY_DELAY) {
}

hyperMuduo::net::Connector::~Connector() {
    resetChannel();
}

void hyperMuduo::net::Connector::start() {
    connect_ = true;
    loop_.runInLoop([self = shared_from_this()]() {
        self->startInLoop();
    });
}

void hyperMuduo::net::Connector::restart() {
    loop_.assertInLoopThread();
    setState(State::Disconnected);
    connect_ = true;
    retry_delay_ = DEFAULT_RETRY_DELAY;
    startInLoop();
}

void hyperMuduo::net::Connector::stop() {
    connect_ = false;
    loop_.runInLoop([self = shared_from_this()]() {
        self->stopInLoop();
    });
}

void hyperMuduo::net::Connector::startInLoop() {
    loop_.assertInLoopThread();
    if (state_ != State::Disconnected) {
        SPDLOG_CRITICAL("Wrong state!Current State:{}", getStateAsString(state_));
        std::abort();
    }
    setState(State::Connecting);
    connect();
}

void hyperMuduo::net::Connector::stopInLoop() {
    loop_.assertInLoopThread();
    if (state_ == State::Connecting) {
        resetChannel();
        socket_.reset();
    }
    setState(State::Disconnected);
}

void hyperMuduo::net::Connector::connect() {
    socket_ = std::make_unique<Socket>();
    switch (int savedErrno = socket_->connect(address_)) {
        case 0:
        case EINPROGRESS:
        case EINTR:
        case EISCONN:
            connecting();
            break;

        case EAGAIN:
        case EADDRINUSE:
        case EADDRNOTAVAIL:
        case ECONNREFUSED:
        case ENETUNREACH:
            retry();
            break;
        default:
            SPDLOG_ERROR("Connector::connect - Unexpected error {}",savedErrno);
            socket_.reset();
            setState(State::Disconnected);
            break;
    }
}

void hyperMuduo::net::Connector::connecting() {
    channel_ = std::make_unique<Channel>(loop_,socket_->getFd());
    channel_->setWriteCallback([self = shared_from_this()]() {
        self->handleWritable();
    });
    channel_->listenTillWritable();
}

void hyperMuduo::net::Connector::handleWritable() {
    int err = socket_->getSocketError();
    if (err != 0) {
        SPDLOG_WARN("Connector::handleWritable - connect error: {}", std::system_category().message(err));
        resetChannel();
        socket_.reset();
        retry();
        return;
    }
    
    state_ = State::Connected;
    resetChannel();
    SPDLOG_DEBUG("Connector::handleWritable - Connection established");

    if (new_connection_callback_) {
        new_connection_callback_(std::move(*socket_));
    }
    socket_.reset();
}

void hyperMuduo::net::Connector::resetChannel() {
    if (channel_) {
        channel_->ignoreAll();
        loop_.removeChannel(*channel_);
        channel_.reset();
    }
}

void hyperMuduo::net::Connector::retry() {
    // Reset state before retrying
    setState(State::Disconnected);
    socket_.reset();
    
    retry_delay_ = std::min(2 * retry_delay_,std::chrono::duration_cast<std::chrono::milliseconds>(MAX_RETRY_DELAY));
    if (connect_ == true) {
        loop_.runAfter(retry_delay_,[self = shared_from_this()]() {
            self->startInLoop();
        });
    }
}




std::string hyperMuduo::net::Connector::getStateAsString(State state) {
    switch (state) {
        case State::Disconnected:
            return "Disconnected";
        case State::Connecting:
            return "Connecting";
        case State::Connected:
            return "Connected";
        default:
            return "Unknown";
    }
}
