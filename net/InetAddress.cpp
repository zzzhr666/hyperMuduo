#include "InetAddress.hpp"

#include <cstring>
#include <spdlog/spdlog.h>

hyperMuduo::net::InetAddress::InetAddress(uint16_t port)
    : InetAddress("0.0.0.0", port) {
}

hyperMuduo::net::InetAddress::InetAddress(const std::string& ip, uint16_t port) {
    std::memset(&addr_, 0, sizeof(addr_));
    addr_.sin_family = AF_INET;
    addr_.sin_port = htons(port);
    if (int ret = inet_pton(AF_INET, ip.c_str(), &addr_.sin_addr.s_addr); ret != 1) {
        if (ret == -1) {
            SPDLOG_ERROR("Invalid af");
        } else if (ret == 0) {
            SPDLOG_ERROR("Invalid ip");
        }
    }
}

hyperMuduo::net::InetAddress::InetAddress(const sockaddr_in& addr)
    : addr_(addr) {
}

std::string hyperMuduo::net::InetAddress::toIp() const {
    char ip[INET_ADDRSTRLEN]{};
    ::inet_ntop(AF_INET, &addr_.sin_addr.s_addr, ip, INET_ADDRSTRLEN);
    return ip;
}

uint16_t hyperMuduo::net::InetAddress::toPort() const {
    return ntohs(addr_.sin_port);
}

std::string hyperMuduo::net::InetAddress::toIpPort() const {
    return toIp() + ':' + std::to_string(toPort());
}

const sockaddr_in* hyperMuduo::net::InetAddress::getSockAddrIn() const {
    return &addr_;
}

sockaddr_in* hyperMuduo::net::InetAddress::getSockAddrInMutable() {
    return &addr_;
}





