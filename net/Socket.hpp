#pragma once



namespace hyperMuduo::net {
class InetAddress;
    class Socket {
    public:
        static constexpr int INVALID_FD = -1;

        Socket();

        explicit Socket(int sock_fd); //接管已有的fd;

        Socket(const Socket&) = delete;

        Socket& operator=(const Socket&) = delete;

        Socket(Socket&& other) noexcept;

        Socket& operator=(Socket&& other) noexcept;

        int getFd() const {
            return socket_fd_;
        }

        void invalidate();

        bool valid()const {
            return socket_fd_ != INVALID_FD;
        }

        Socket accept(InetAddress& addr);

        void setTcpNoDelay(bool on);

        void setKeepAlive(bool on);

        void bindAddress(const InetAddress& addr);

        void listen();

        void setReuseAddr();

        void setReusePort();

        void setKeepAlive();

        int connect(const InetAddress& addr);

        InetAddress getLocalAddress()const;

        InetAddress getPeerAddress()const;

        int getSocketError()const;

        void shutdownWrite();

        ~Socket();

    private:
        int socket_fd_;
    };
}
