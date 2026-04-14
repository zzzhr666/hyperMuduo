#include <gtest/gtest.h>
#include <sys/socket.h>
#include <unistd.h>
#include <thread>
#include <chrono>
#include <atomic>
#include <string>
#include "net/TcpServer.hpp"
#include "net/EventLoop.hpp"
#include "net/InetAddress.hpp"
#include "net/Buffer.hpp"

using namespace hyperMuduo::net;

namespace {
// 辅助函数：创建客户端TCP连接
int create_client_connection(uint16_t port) {
    int client_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (client_fd < 0) return -1;

    sockaddr_in server_addr{};
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    inet_pton(AF_INET, "127.0.0.1", &server_addr.sin_addr);

    int ret = connect(client_fd, reinterpret_cast<sockaddr*>(&server_addr), sizeof(server_addr));
    if (ret < 0) {
        close(client_fd);
        return -1;
    }
    return client_fd;
}

// 发送数据到服务器
ssize_t send_data(int fd, const std::string& data) {
    return ::send(fd, data.data(), data.size(), 0);
}

// 从服务器接收数据
std::string recv_data(int fd, size_t max_bytes = 4096) {
    std::string buffer(max_bytes, 0);
    ssize_t n = ::recv(fd, buffer.data(), max_bytes, 0);
    if (n > 0) {
        buffer.resize(n);
        return buffer;
    }
    return "";
}
} // namespace

// Test 1: 服务器启动和停止
TEST(TcpServerTest, StartAndStop) {
    EventLoop loop;
    InetAddress addr(20000);
    TcpServer server(loop, addr, "TestServer");

    std::atomic<bool> server_started{false};
    server.setConnectionCallback([&](const std::shared_ptr<TcpConnection>& conn) {
        if (conn->connected()) {
            server_started = true;
        }
    });

    server.start();

    // 运行循环一段时间后停止
    std::thread stopper([&]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        loop.quit();
    });

    loop.loop();
    stopper.join();

    // 服务器应该已启动
    EXPECT_TRUE(server_started.load() == false); // 没有连接时不触发
}

// Test 2: 单连接回声测试 (Echo Server)
TEST(TcpServerTest, EchoServerSingleConnection) {
    EventLoop loop;
    InetAddress addr(20001);
    TcpServer server(loop, addr, "EchoServer");

    // 设置消息回调：将收到的数据原样发回
    server.setMessageCallback([&](const std::shared_ptr<TcpConnection>& conn,
                                   Buffer& buf,
                                   std::chrono::system_clock::time_point) {
        std::string msg = buf.retrieveAllAsString();
        conn->send(msg); // 回声
    });

    server.start();

    std::atomic<bool> echo_received{false};
    std::string received_msg;

    // 从客户端连接并发送数据
    std::thread client([&]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));

        int client_fd = create_client_connection(20001);
        ASSERT_GE(client_fd, 0);

        // 发送测试数据
        std::string test_msg = "Hello, HyperMuduo!";
        ssize_t n = send_data(client_fd, test_msg);
        EXPECT_EQ(n, test_msg.size());

        // 接收回声
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        received_msg = recv_data(client_fd);

        if (received_msg == test_msg) {
            echo_received = true;
        }

        close(client_fd);
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        loop.quit();
    });

    loop.loop();
    client.join();

    EXPECT_TRUE(echo_received.load());
    EXPECT_EQ(received_msg, "Hello, HyperMuduo!");
}

// Test 3: 多连接并发
TEST(TcpServerTest, MultipleConcurrentConnections) {
    EventLoop loop;
    InetAddress addr(20002);
    TcpServer server(loop, addr, "MultiServer");

    std::atomic<int> connection_count{0};
    std::atomic<int> message_count{0};
    constexpr int num_clients = 3;

    // 设置连接回调
    server.setConnectionCallback([&](const std::shared_ptr<TcpConnection>& conn) {
        if (conn->connected()) {
            connection_count++;
        }
    });

    // 设置消息回调
    server.setMessageCallback([&](const std::shared_ptr<TcpConnection>& conn,
                                   Buffer& buf,
                                   std::chrono::system_clock::time_point) {
        std::string msg = buf.retrieveAllAsString();
        message_count++;
        conn->send("Echo: " + msg);
    });

    server.start();

    // 创建多个客户端
    std::thread clients([&]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));

        std::vector<int> client_fds;
        for (int i = 0; i < num_clients; i++) {
            int fd = create_client_connection(20002);
            if (fd >= 0) {
                client_fds.push_back(fd);
                send_data(fd, "Msg from client " + std::to_string(i));
            }
        }

        // 等待回声
        std::this_thread::sleep_for(std::chrono::milliseconds(200));

        for (int fd : client_fds) {
            std::string response = recv_data(fd);
            EXPECT_FALSE(response.empty());
            close(fd);
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        loop.quit();
    });

    loop.loop();
    clients.join();

    EXPECT_EQ(connection_count.load(), num_clients);
    EXPECT_EQ(message_count.load(), num_clients);
}

// Test 4: 连接回调触发
TEST(TcpServerTest, ConnectionCallbackTriggers) {
    EventLoop loop;
    InetAddress addr(20003);
    TcpServer server(loop, addr, "ConnCallbackServer");

    std::atomic<int> connect_events{0};
    std::atomic<int> disconnect_events{0};

    server.setConnectionCallback([&](const std::shared_ptr<TcpConnection>& conn) {
        if (conn->connected()) {
            connect_events++;
        } else {
            disconnect_events++;
        }
    });

    server.start();

    // 创建并断开连接
    std::thread client([&]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));

        int client_fd = create_client_connection(20003);
        ASSERT_GE(client_fd, 0);

        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        close(client_fd);

        std::this_thread::sleep_for(std::chrono::milliseconds(200));
        loop.quit();
    });

    loop.loop();
    client.join();

    EXPECT_GE(connect_events.load(), 1);
    EXPECT_GE(disconnect_events.load(), 1);
}

// Test 5: 消息回调接收完整数据
TEST(TcpServerTest, MessageCallbackReceivesData) {
    EventLoop loop;
    InetAddress addr(20004);
    TcpServer server(loop, addr, "MsgCallbackServer");

    std::atomic<bool> data_received{false};
    std::string received_data;

    server.setMessageCallback([&](const std::shared_ptr<TcpConnection>&,
                                   Buffer& buf,
                                   std::chrono::system_clock::time_point) {
        received_data = buf.retrieveAllAsString();
        data_received = true;
    });

    server.start();

    std::thread client([&]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));

        int client_fd = create_client_connection(20004);
        ASSERT_GE(client_fd, 0);

        std::string test_data = "Test message with some content";
        send_data(client_fd, test_data);

        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        close(client_fd);

        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        loop.quit();
    });

    loop.loop();
    client.join();

    EXPECT_TRUE(data_received.load());
    EXPECT_EQ(received_data, "Test message with some content");
}
