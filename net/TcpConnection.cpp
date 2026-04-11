#include "TcpConnection.hpp"

#include <spdlog/spdlog.h>
#include <system_error>

#include "Buffer.hpp"
#include "Channel.hpp"
#include "EventLoop.hpp"
#include "Socket.hpp"

namespace hyperMuduo::net {


    TcpConnection::TcpConnection(EventLoop& loop,
                                 std::string_view name,
                                 Socket socket,
                                 InetAddress local_addr,
                                 InetAddress peer_addr)
        : loop_(loop),
          name_(name),
          state_(State::Connecting),
          socket_(std::make_unique<Socket>(std::move(socket))),
          channel_(std::make_unique<Channel>(loop, socket_->getFd())),
          local_address_(std::move(local_addr)),
          peer_address_(std::move(peer_addr)),
          receive_buffer_(std::make_unique<Buffer>()),
          send_buffer_(std::make_unique<Buffer>()) {
        // 注意：多线程模式下，TcpConnection 对象由主线程创建
        // 所以这里不能 assertInLoopThread()，也不能注册 Channel
        channel_->setReadCallback([this](std::chrono::system_clock::time_point tp) {
            handleReadable(tp);
        });
        // listenTillReadable() 移至 connectionEstablished() 中在子线程执行
    }

    void TcpConnection::shutdown() {
        if (state_ == State::Connected) {
            setState(State::Disconnecting);
            loop_.runInLoop([self = shared_from_this()]() {
                self->shutdownInLoop();
            });
        }
    }

    void TcpConnection::setTcpNoDelay(bool on) {
        socket_->setTcpNoDelay(on);
    }

    void TcpConnection::setKeepAlive(bool on) {
        socket_->setKeepAlive(on);
    }

    void TcpConnection::send(Buffer& buffer) {
        if (state_ == State::Connected) {
            if (loop_.isInLoopThread()) {
                sendInLoop(buffer);
            } else {
                // 跨线程：将用户 buffer 的数据转移到堆上，避免拷贝
                auto msg = std::make_shared<Buffer>();
                msg->swap(buffer); // 零拷贝交换

                loop_.runInLoop([self = shared_from_this(), msg] {
                    self->sendInLoop(*msg);
                });
            }
        }
    }

    void TcpConnection::send(std::string msg) {
        if (state_ == State::Connected) {
            if (loop_.isInLoopThread()) {
                sendInLoop(std::move(msg));
            } else {
                loop_.runInLoop([self = shared_from_this(),msg = std::move(msg)]() {
                    self->sendInLoop(std::move(msg));
                });
            }
        }
    }

    void TcpConnection::sendInLoop(const std::string& msg) {
        sendInLoop(msg.data(), msg.size());
    }

    void TcpConnection::sendInLoop(const char* msg, size_t size) {
        loop_.assertInLoopThread();
        ssize_t n_write{};
        if (!channel_->isWriting() && send_buffer_->readableBytes() == 0) {
            n_write = ::write(channel_->getFd(), msg, size);
            if (n_write >= 0) {
                if (n_write < size) {
                    SPDLOG_TRACE("Writing task don't finish.");
                } else if (write_complete_callback_) {
                    loop_.queueInLoop([self = shared_from_this()]() {
                        self->write_complete_callback_(self);
                    });
                }
            } else {
                n_write = 0;
                if (errno != EWOULDBLOCK) {
                    SPDLOG_ERROR("Write error: {}", std::system_category().message(errno));
                }
            }
        }
        if (n_write < size) {
            send_buffer_->append(msg + n_write, size - n_write);
            if (!channel_->isWriting()) {
                channel_->listenTillWritable();
            }
            // 检查高水位
            if (high_water_callback_ && send_buffer_->readableBytes() >= high_water_mark_) {
                loop_.queueInLoop([self = shared_from_this()]() {
                    self->high_water_callback_(self->shared_from_this());
                });
            }
        }
    }

    void TcpConnection::sendInLoop(Buffer& buffer) {
        loop_.assertInLoopThread();
        ssize_t n_write = 0;

        // 1. 如果 send_buffer_ 为空且未在写，尝试直接发送
        if (!channel_->isWriting() && send_buffer_->readableBytes() == 0) {
            n_write = ::write(socket_->getFd(), buffer.peek(), buffer.readableBytes());
            if (n_write >= 0) {
                buffer.retrieve(n_write);
            } else {
                n_write = 0;
                if (errno != EWOULDBLOCK) {
                    SPDLOG_ERROR("Write error: {}", std::system_category().message(errno));
                }
            }
        }

        // 2. 处理未发送完的数据
        if (buffer.readableBytes() > 0) {
            if (send_buffer_->readableBytes() == 0) {
                send_buffer_->swap(buffer); // 零拷贝转移
            } else {
                send_buffer_->append(buffer.peek(), buffer.readableBytes());
                buffer.retrieveAll();
            }

            // 检查高水位
            if (high_water_callback_ && send_buffer_->readableBytes() >= high_water_mark_) {
                loop_.queueInLoop([self = shared_from_this()]() {
                    self->high_water_callback_(self);
                });
            }

            if (!channel_->isWriting()) {
                channel_->listenTillWritable();
            }
        }
    }

    void TcpConnection::shutdownInLoop() {
        loop_.assertInLoopThread();
        if (state_ == State::Connected) {
            setState(State::Disconnecting);
            if (!channel_->isWriting()) {
                socket_->shutdownWrite();
            }
        }

    }

    Buffer& TcpConnection::receiveBuffer() {
        return *receive_buffer_;
    }

    Buffer& TcpConnection::sendBuffer() {
        return *send_buffer_;
    }

    void TcpConnection::connectionEstablished() {
        loop_.assertInLoopThread();
        if (state_ != State::Connecting) {
            SPDLOG_CRITICAL("State Error: current state={}", getState());
            std::abort();
        }
        setState(State::Connected);
        channel_->tie(shared_from_this());
        channel_->listenTillReadable();

        if (connection_callback_) {
            connection_callback_(shared_from_this());
        }
    }

    void TcpConnection::connectionDestroyed() {
        loop_.assertInLoopThread();
        setState(State::Disconnected);
        channel_->ignoreAll();
        if (connection_callback_) {
            connection_callback_(shared_from_this());
        }
        loop_.removeChannel(*channel_);
    }

    void TcpConnection::setContext(std::any context) {
        context_ = std::move(context);
    }

    std::string TcpConnection::getState() const {
        switch (state_) {
            case State::Connecting:
                return "Connecting";
            case State::Connected:
                return "Connected";
            case State::Disconnected:
                return "Disconnected";
            default:
                return "Invalid state";
        }
    }

    void TcpConnection::handleReadable(std::chrono::system_clock::time_point time) {
        loop_.assertInLoopThread();
        int savedErrno{};
        ssize_t n_read = receive_buffer_->readFd(socket_->getFd(), &savedErrno);
        if (n_read > 0) {
            if (message_callback_) {
                message_callback_(shared_from_this(), *receive_buffer_, time);
            }
        } else if (n_read == 0) {
            handleClose();
        } else {
            errno = savedErrno;
            SPDLOG_ERROR("TcpConnection::handleReadable()");
            handleError();
        }

    }

    void TcpConnection::handleWritable() {
        loop_.assertInLoopThread();
        if (channel_->isWriting()) {
            ssize_t n_write = ::write(channel_->getFd(), send_buffer_->peek(), send_buffer_->readableBytes());
            if (n_write > 0) {
                send_buffer_->retrieve(n_write);
                if (send_buffer_->readableBytes() == 0) {
                    channel_->ignoreWritable();
                    if (write_complete_callback_) {
                        loop_.queueInLoop([self = shared_from_this()]() {
                            self->write_complete_callback_(self);
                        });
                    }
                    if (state_ == State::Disconnecting) {
                        shutdownInLoop();
                    }
                } else {
                    SPDLOG_TRACE("writing task don't finish");
                }
            } else {
                SPDLOG_ERROR("TcpConnection::handlewrite");
            }
        } else {
            SPDLOG_TRACE("Connection is Down,No more writing");
        }
    }

    void TcpConnection::handleClose() {
        loop_.assertInLoopThread();
        if (state_ != State::Connected && state_ != State::Disconnecting) {
            SPDLOG_CRITICAL("state error:{}", getState());
            std::abort();
        }
        setState(State::Disconnected);

        SPDLOG_TRACE("TcpConnection::handleClose() state = {}", state_);
        channel_->ignoreAll();
        close_callback_(shared_from_this());
    }

    void TcpConnection::handleError() {
        int err = socket_->getSocketError();
        SPDLOG_ERROR("TcpConnection::handleError [{}] - SO_ERROR={},error msg： {}", name_, err,
                     std::system_category().message(err));
    }
} // namespace hyper::net
