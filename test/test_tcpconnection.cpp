#include <gtest/gtest.h>
#include <sys/socket.h>
#include <unistd.h>
#include <thread>
#include <chrono>
#include <atomic>
#include <string>
#include "net/TcpConnection.hpp"
#include "net/EventLoop.hpp"
#include "net/Socket.hpp"
#include "net/InetAddress.hpp"
#include "net/Buffer.hpp"

using namespace hyperMuduo::net;

// 辅助：创建一对已连接的socket
std::pair<int, int> create_connected_sockets() {
    int listener = socket(AF_INET, SOCK_STREAM, 0);
    EXPECT_GE(listener, 0);

    int opt = 1;
    setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port = htons(0); // 随机端口

    EXPECT_EQ(bind(listener, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)), 0);
    EXPECT_EQ(listen(listener, 1), 0);

    // 获取实际端口
    socklen_t len = sizeof(addr);
    getsockname(listener, reinterpret_cast<sockaddr*>(&addr), &len);

    int client_fd = socket(AF_INET, SOCK_STREAM, 0);
    EXPECT_GE(client_fd, 0);

    EXPECT_EQ(connect(client_fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)), 0);

    int server_fd = accept(listener, nullptr, nullptr);
    EXPECT_GE(server_fd, 0);

    close(listener);
    return {server_fd, client_fd};
}

// Test 1: 连接建立流程 - 验证状态转换
TEST(TcpConnectionTest, ConnectionEstablishedStateTransition) {
    EventLoop loop;
    auto [server_fd, client_fd] = create_connected_sockets();

    Socket server_socket(server_fd);
    InetAddress local_addr(server_socket.getLocalAddress());
    InetAddress peer_addr("127.0.0.1", 0);

    auto conn = std::make_shared<TcpConnection>(loop, "test#1", std::move(server_socket), local_addr, peer_addr);

    // 初始状态应该是 Connecting
    EXPECT_TRUE(conn->connected() == false);

    std::atomic<bool> callback_triggered{false};
    conn->setConnectionCallback([&](const std::shared_ptr<TcpConnection>& c) {
        if (c->connected()) {
            callback_triggered = true;
        }
    });

    // 触发连接建立
    conn->connectionEstablished();

    // 验证状态已转换为 Connected
    EXPECT_TRUE(conn->connected());
    EXPECT_TRUE(callback_triggered.load());

    // 清理
    conn->connectionDestroyed();
    close(client_fd);
}

// Test 2: 同步发送数据（同线程）
TEST(TcpConnectionTest, SendDataInSameThread) {
    EventLoop loop;
    auto [server_fd, client_fd] = create_connected_sockets();

    Socket server_socket(server_fd);
    InetAddress local_addr(server_socket.getLocalAddress());
    InetAddress peer_addr("127.0.0.1", 0);

    auto conn = std::make_shared<TcpConnection>(loop, "test#2", std::move(server_socket), local_addr, peer_addr);
    conn->connectionEstablished();

    std::string test_msg = "Hello from TcpConnection!";
    conn->send(test_msg);

    // 在客户端接收数据
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    char buffer[256] = {0};
    ssize_t n = recv(client_fd, buffer, sizeof(buffer), 0);

    EXPECT_GT(n, 0);
    EXPECT_EQ(std::string(buffer, n), test_msg);

    conn->connectionDestroyed();
    close(client_fd);
}

// Test 3: 跨线程发送数据（验证线程安全）
TEST(TcpConnectionTest, SendDataFromOtherThread) {
    EventLoop loop;
    auto [server_fd, client_fd] = create_connected_sockets();

    Socket server_socket(server_fd);
    InetAddress local_addr(server_socket.getLocalAddress());
    InetAddress peer_addr("127.0.0.1", 0);

    auto conn = std::make_shared<TcpConnection>(loop, "test#3", std::move(server_socket), local_addr, peer_addr);
    conn->connectionEstablished();

    std::atomic<bool> data_received{false};
    std::string received_data;

    // 从另一个线程发送数据
    std::thread sender([&]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
        conn->send("Cross-thread message");
    });

    // 启动事件循环来处理异步发送
    std::thread loop_runner([&]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        loop.quit();
    });

    loop.loop();
    sender.join();
    loop_runner.join();

    // 接收数据
    char buffer[256] = {0};
    ssize_t n = recv(client_fd, buffer, sizeof(buffer), 0);
    if (n > 0) {
        received_data = std::string(buffer, n);
        data_received = true;
    }

    EXPECT_TRUE(data_received.load());
    EXPECT_EQ(received_data, "Cross-thread message");

    conn->connectionDestroyed();
    close(client_fd);
}

// Test 4: 优雅关闭连接
TEST(TcpConnectionTest, GracefulShutdown) {
    EventLoop loop;
    auto [server_fd, client_fd] = create_connected_sockets();

    Socket server_socket(server_fd);
    InetAddress local_addr(server_socket.getLocalAddress());
    InetAddress peer_addr("127.0.0.1", 0);

    auto conn = std::make_shared<TcpConnection>(loop, "test#4", std::move(server_socket), local_addr, peer_addr);
    conn->connectionEstablished();

    // 验证连接已建立
    EXPECT_TRUE(conn->connected());

    // 发送一些数据
    conn->send("Before shutdown");

    // 触发关闭 - 验证不崩溃
    conn->shutdown();

    // 验证状态已切换到 Disconnecting
    EXPECT_FALSE(conn->connected());

    // 清理
    close(client_fd);
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    // 销毁连接
    conn->connectionDestroyed();
    EXPECT_FALSE(conn->connected()); // Disconnected
}

// Test 5: Context 上下文存储
TEST(TcpConnectionTest, ContextStorage) {
    EventLoop loop;
    auto [server_fd, client_fd] = create_connected_sockets();

    Socket server_socket(server_fd);
    InetAddress local_addr(server_socket.getLocalAddress());
    InetAddress peer_addr("127.0.0.1", 0);

    auto conn = std::make_shared<TcpConnection>(loop, "test#5", std::move(server_socket), local_addr, peer_addr);

    // 验证两个 buffer 都已初始化
    EXPECT_GT(conn->sendBuffer().writableBytes(), 0);
    EXPECT_GT(conn->receiveBuffer().writableBytes(), 0);

    // 测试存储不同类型的上下文
    struct SessionData {
        std::string user_id;
        int login_time;
    };

    // 存储上下文
    SessionData session{"user123", 1000};
    conn->setContext(std::make_any<SessionData>(session));

    // 验证上下文存在
    EXPECT_TRUE(conn->hasContext<SessionData>());

    // 读取上下文
    const auto& retrieved = conn->getContextAs<SessionData>();
    EXPECT_EQ(retrieved.user_id, "user123");
    EXPECT_EQ(retrieved.login_time, 1000);

    // 修改上下文
    SessionData new_session{"user456", 2000};
    conn->setContext(std::make_any<SessionData>(new_session));
    const auto& modified = conn->getContextAs<SessionData>();
    EXPECT_EQ(modified.user_id, "user456");

    // 测试错误类型
    EXPECT_FALSE(conn->hasContext<int>());

    close(client_fd);
}
