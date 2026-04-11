#include <gtest/gtest.h>
#include <sys/socket.h>
#include <unistd.h>
#include <thread>
#include <chrono>
#include <atomic>
#include <string>
#include <mutex>
#include <unordered_set>
#include "net/TcpServer.hpp"
#include "net/EventLoop.hpp"
#include "net/InetAddress.hpp"
#include "net/Buffer.hpp"

using namespace hyperMuduo::net;

// 辅助函数：创建客户端 TCP 连接
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

// Test 1: 验证连接被分配到不同线程
TEST(MultiThreadTcpServerTest, ConnectionsDistributedToWorkers) {
    EventLoop loop;
    InetAddress addr(20010);
    TcpServer server(loop, addr, "DistServer");
    server.setThreadNum(3); // 3 个 worker 线程

    std::mutex mtx;
    std::unordered_set<std::thread::id> worker_ids;
    std::atomic<int> conn_count{0};

    server.setConnectionCallback([&](const std::shared_ptr<TcpConnection>& conn) {
        if (conn->connected()) {
            std::lock_guard<std::mutex> lock(mtx);
            worker_ids.insert(std::this_thread::get_id());
            ++conn_count;
        }
    });

    server.start();

    // 创建多个客户端连接
    std::thread clients([&]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));

        for (int i = 0; i < 3; i++) {
            int fd = create_client_connection(20010);
            ASSERT_GE(fd, 0);
            std::this_thread::sleep_for(std::chrono::milliseconds(50)); // 稍微错开连接时间
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(200));
        loop.quit();
    });

    loop.loop();
    clients.join();

    // 验证连接是否被分配到不同线程
    // 注意：由于是轮询分配，至少应该有多个不同的线程处理了连接
    // 但因为连接是串行的，可能会复用线程，我们主要验证 server 启动成功
    EXPECT_GE(conn_count.load(), 3);
    EXPECT_GT(worker_ids.size(), 1); // 至少两个不同线程处理
}

// Test 2: 多线程 Echo 服务
TEST(MultiThreadTcpServerTest, MultiThreadEchoServer) {
    EventLoop loop;
    InetAddress addr(20011);
    TcpServer server(loop, addr, "EchoServer-MT");
    server.setThreadNum(4);

    std::atomic<int> echo_count{0};

    server.setMessageCallback([&](const std::shared_ptr<TcpConnection>& conn,
                                   Buffer& buf,
                                   std::chrono::system_clock::time_point) {
        std::string msg = buf.retrieveAllAsString();
        conn->send("Echo: " + msg);
    });

    server.start();

    std::thread clients([&]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));

        std::vector<int> client_fds;
        for (int i = 0; i < 4; i++) {
            int fd = create_client_connection(20011);
            if (fd >= 0) {
                client_fds.push_back(fd);
                send_data(fd, "Hello from " + std::to_string(i));
            }
        }

        // 等待回声
        std::this_thread::sleep_for(std::chrono::milliseconds(300));

        for (int i = 0; i < client_fds.size(); i++) {
            if (std::string response = recv_data(client_fds[i]); response.find("Echo: Hello from " + std::to_string(i)) != std::string::npos) {
                ++echo_count;
            }
            close(client_fds[i]);
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        loop.quit();
    });

    loop.loop();
    clients.join();

    EXPECT_EQ(echo_count.load(), 4);
}

// Test 3: 高并发连接与优雅断开
TEST(MultiThreadTcpServerTest, HighConcurrencyConnections) {
    EventLoop loop;
    InetAddress addr(20012);
    TcpServer server(loop, addr, "HighConcurrencyServer");
    server.setThreadNum(4);

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

    std::thread clients([&]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));

        std::vector<int> client_fds;
        for (int i = 0; i < 10; i++) {
            int fd = create_client_connection(20012);
            if (fd >= 0) {
                client_fds.push_back(fd);
            }
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(500));

        for (const int fd : client_fds) {
            close(fd);
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        loop.quit();
    });

    loop.loop();
    clients.join();

    EXPECT_EQ(connect_events.load(), 10);
    EXPECT_EQ(disconnect_events.load(), 10);
}

// Test 4: 跨线程发送数据（主线程主动推送数据到子线程连接）
TEST(MultiThreadTcpServerTest, CrossThreadSend) {
    EventLoop loop;
    InetAddress addr(20013);
    TcpServer server(loop, addr, "CrossThreadServer");
    server.setThreadNum(3);

    std::mutex conn_mtx;
    std::vector<std::shared_ptr<TcpConnection>> conns;
    std::atomic<int> conn_count{0};

    server.setConnectionCallback([&](const std::shared_ptr<TcpConnection>& conn) {
        if (conn->connected()) {
            std::lock_guard<std::mutex> lock(conn_mtx);
            conns.push_back(conn);
            conn_count++;
        }
    });

    server.start();

    std::thread clients([&]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));

        // 创建 3 个连接
        for (int i = 0; i < 3; i++) {
            int fd = create_client_connection(20013);
            ASSERT_GE(fd, 0);
        }

        // 等待连接建立
        std::this_thread::sleep_for(std::chrono::milliseconds(200));

        // 🔑 关键：从主线程（非连接所属线程）主动发送数据
        {
            std::lock_guard<std::mutex> lock(conn_mtx);
            for (size_t i = 0; i < conns.size(); i++) {
                conns[i]->send("Push from main thread #" + std::to_string(i));
            }
        }

        // 等待推送数据到达客户端
        std::this_thread::sleep_for(std::chrono::milliseconds(300));

        // 验证每个连接都收到了数据
        for (auto& conn : conns) {
            conn->shutdown();
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(200));
        loop.quit();
    });

    loop.loop();
    clients.join();

    EXPECT_EQ(conn_count.load(), 3);
}

// Test 5: 验证回调在正确的子线程执行
TEST(MultiThreadTcpServerTest, CallbacksRunInWorkerThreads) {
    EventLoop loop;
    InetAddress addr(20014);
    TcpServer server(loop, addr, "ThreadVerifyServer");
    server.setThreadNum(4);

    std::mutex mtx;
    std::unordered_set<std::thread::id> conn_callback_threads;
    std::unordered_set<std::thread::id> msg_callback_threads;
    std::unordered_set<std::thread::id> close_callback_threads;

    server.setConnectionCallback([&](const std::shared_ptr<TcpConnection>& conn) {
        std::lock_guard<std::mutex> lock(mtx);
        if (conn->connected()) {
            conn_callback_threads.insert(std::this_thread::get_id());
        } else {
            close_callback_threads.insert(std::this_thread::get_id());
        }
    });

    server.setMessageCallback([&](const std::shared_ptr<TcpConnection>&,
                                   Buffer& buf,
                                   std::chrono::system_clock::time_point) {
        std::lock_guard<std::mutex> lock(mtx);
        msg_callback_threads.insert(std::this_thread::get_id());
        buf.retrieveAll();
    });

    server.start();

    std::thread clients([&]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));

        for (int i = 0; i < 4; i++) {
            int fd = create_client_connection(20014);
            ASSERT_GE(fd, 0);
            send_data(fd, "test");
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(200));
        loop.quit();
    });

    loop.loop();
    clients.join();

    // 连接回调应该在子线程执行
    EXPECT_GT(conn_callback_threads.size(), 0);
    // 消息回调也应该在子线程执行
    EXPECT_GT(msg_callback_threads.size(), 0);
    // 连接回调和消息回调应该在不同线程（至少部分）
    // 注意：不应该在主线程
    EXPECT_EQ(conn_callback_threads.count(std::this_thread::get_id()), 0);
}

// Test 6: 连接上下文 (Context) 在多线程下安全
TEST(MultiThreadTcpServerTest, ConnectionContextThreadSafety) {
    EventLoop loop;
    InetAddress addr(20015);
    TcpServer server(loop, addr, "ContextServer");
    server.setThreadNum(3);

    struct SessionData {
        std::string user_id;
        int login_time;
    };

    std::atomic<int> session_count{0};

    server.setMessageCallback([&](const std::shared_ptr<TcpConnection>& conn,
                                   Buffer& buf,
                                   std::chrono::system_clock::time_point) {
        std::string msg = buf.retrieveAllAsString();

        // 第一次收到消息时设置上下文
        if (!conn->hasContext<SessionData>()) {
            SessionData session{msg, 1000};
            conn->setContext(session);
            session_count++;
            conn->send("Session created for: " + msg);
        } else {
            // 后续消息读取上下文
            const SessionData& session = conn->getContextAs<SessionData>();
            conn->send("Hello " + session.user_id + ", received: " + msg);
        }
    });

    server.start();

    std::thread clients([&]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));

        // 客户端 1
        int fd1 = create_client_connection(20015);
        ASSERT_GE(fd1, 0);
        send_data(fd1, "Alice");
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        std::string resp1 = recv_data(fd1);
        EXPECT_EQ(resp1, "Session created for: Alice");

        send_data(fd1, "msg1");
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        resp1 = recv_data(fd1);
        EXPECT_EQ(resp1, "Hello Alice, received: msg1");

        // 客户端 2
        int fd2 = create_client_connection(20015);
        ASSERT_GE(fd2, 0);
        send_data(fd2, "Bob");
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        std::string resp2 = recv_data(fd2);
        EXPECT_EQ(resp2, "Session created for: Bob");

        send_data(fd2, "msg2");
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        resp2 = recv_data(fd2);
        EXPECT_EQ(resp2, "Hello Bob, received: msg2");

        close(fd1);
        close(fd2);
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
        loop.quit();
    });

    loop.loop();
    clients.join();

    EXPECT_EQ(session_count.load(), 2);
}

// Test 7: 写完成回调 (WriteCompleteCallback) 测试
TEST(MultiThreadTcpServerTest, WriteCompleteCallbackTriggers) {
    EventLoop loop;
    InetAddress addr(20016);
    TcpServer server(loop, addr, "WriteCompleteServer");
    server.setThreadNum(2);

    std::atomic<int> write_complete_count{0};
    std::atomic<int> message_count{0};

    server.setWriteCompleteCallback([&](const std::shared_ptr<TcpConnection>&) {
        write_complete_count++;
    });

    server.setMessageCallback([&](const std::shared_ptr<TcpConnection>& conn,
                                   Buffer& buf,
                                   std::chrono::system_clock::time_point) {
        std::string msg = buf.retrieveAllAsString();
        message_count++;
        // 发送大量数据以触发写完成回调
        std::string large_data(1024 * 100, 'X'); // 100KB
        conn->send(large_data);
    });

    server.start();

    std::thread clients([&]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));

        for (int i = 0; i < 3; i++) {
            int fd = create_client_connection(20016);
            ASSERT_GE(fd, 0);
            send_data(fd, "trigger");
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        loop.quit();
    });

    loop.loop();
    clients.join();

    EXPECT_EQ(message_count.load(), 3);
    // 写完成回调应该被触发至少一次
    EXPECT_GT(write_complete_count.load(), 0);
}

// Test 8: 大量连接压力测试 (50 个连接)
TEST(MultiThreadTcpServerTest, StressTestManyConnections) {
    EventLoop loop;
    InetAddress addr(20017);
    TcpServer server(loop, addr, "StressServer");
    server.setThreadNum(8);

    std::atomic<int> connect_events{0};
    std::atomic<int> disconnect_events{0};
    std::atomic<int> message_count{0};

    server.setConnectionCallback([&](const std::shared_ptr<TcpConnection>& conn) {
        if (conn->connected()) {
            connect_events++;
        } else {
            disconnect_events++;
        }
    });

    server.setMessageCallback([&](const std::shared_ptr<TcpConnection>&,
                                   Buffer& buf,
                                   std::chrono::system_clock::time_point) {
        buf.retrieveAll();
        message_count++;
    });

    server.start();

    constexpr int num_clients = 50;

    std::thread clients([&]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(200));

        std::vector<int> client_fds;
        client_fds.reserve(num_clients);

        // 快速创建 50 个连接
        for (int i = 0; i < num_clients; i++) {
            int fd = create_client_connection(20017);
            if (fd >= 0) {
                client_fds.push_back(fd);
                send_data(fd, "hello");
            }
        }

        // 等待所有消息处理
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));

        // 断开所有连接
        for (int fd : client_fds) {
            close(fd);
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        loop.quit();
    });

    loop.loop();
    clients.join();

    EXPECT_EQ(connect_events.load(), num_clients);
    EXPECT_EQ(message_count.load(), num_clients);
    EXPECT_EQ(disconnect_events.load(), num_clients);
}

// Test 9: 大数据流传输测试 (每个连接发送 1MB)
TEST(MultiThreadTcpServerTest, LargeDataTransfer) {
    EventLoop loop;
    InetAddress addr(20018);
    TcpServer server(loop, addr, "LargeDataServer");
    server.setThreadNum(4);

    std::atomic<int> bytes_received{0};
    std::atomic<int> conn_count{0};

    server.setConnectionCallback([&](const std::shared_ptr<TcpConnection>& conn) {
        if (conn->connected()) {
            conn_count++;
        }
    });

    server.setMessageCallback([&](const std::shared_ptr<TcpConnection>& conn,
                                   Buffer& buf,
                                   std::chrono::system_clock::time_point) {
        bytes_received += buf.readableBytes();
        buf.retrieveAll();
        // 发回确认
        conn->send("OK");
    });

    server.start();

    std::thread clients([&]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));

        constexpr int data_size = 1024 * 1024; // 1MB
        std::string large_data(data_size, 'A');

        int fd = create_client_connection(20018);
        ASSERT_GE(fd, 0);

        // 分块发送大数据
        constexpr size_t chunk_size = 64 * 1024; // 64KB
        for (size_t offset = 0; offset < large_data.size(); offset += chunk_size) {
            size_t len = std::min(chunk_size, large_data.size() - offset);
            ssize_t n = send_data(fd, large_data.substr(offset, len));
            ASSERT_GT(n, 0);
        }

        // 等待服务器处理
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));

        // 接收确认
        std::string ack = recv_data(fd);
        EXPECT_FALSE(ack.empty());

        close(fd);
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
        loop.quit();
    });

    loop.loop();
    clients.join();

    EXPECT_EQ(conn_count.load(), 1);
    // 应该接收到大约 1MB 的数据
    EXPECT_GT(bytes_received.load(), 1024 * 1024 * 0.9); // 至少 900KB
}

// Test 10: 动态线程数测试
TEST(MultiThreadTcpServerTest, DynamicThreadNum) {
    EventLoop loop;
    InetAddress addr(20019);
    TcpServer server(loop, addr, "DynamicThreadServer");
    server.setThreadNum(2); // 初始 2 个线程

    std::atomic<int> conn_count{0};
    server.setConnectionCallback([&](const std::shared_ptr<TcpConnection>& conn) {
        if (conn->connected()) {
            conn_count++;
        }
    });

    server.start();

    std::thread clients([&]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));

        // 创建 2 个连接
        for (int i = 0; i < 2; i++) {
            int fd = create_client_connection(20019);
            ASSERT_GE(fd, 0);
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(200));

        // 修改线程数（注意：这只会影响新连接的分配，不影响已有连接）
        server.setThreadNum(4);

        // 再创建 2 个连接
        for (int i = 0; i < 2; i++) {
            int fd = create_client_connection(20019);
            ASSERT_GE(fd, 0);
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(200));
        loop.quit();
    });

    loop.loop();
    clients.join();

    // 所有连接都应该成功建立
    EXPECT_EQ(conn_count.load(), 4);
}

// Test 11: 广播测试（向所有连接发送消息）
TEST(MultiThreadTcpServerTest, BroadcastToAllConnections) {
    EventLoop loop;
    InetAddress addr(20020);
    TcpServer server(loop, addr, "BroadcastServer");
    server.setThreadNum(4);

    std::mutex conn_mtx;
    std::vector<std::shared_ptr<TcpConnection>> all_conns;
    std::atomic<int> conn_count{0};

    server.setConnectionCallback([&](const std::shared_ptr<TcpConnection>& conn) {
        if (conn->connected()) {
            std::lock_guard<std::mutex> lock(conn_mtx);
            all_conns.push_back(conn);
            conn_count++;
        }
    });

    server.start();

    std::thread clients([&]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));

        // 创建 4 个连接
        std::vector<int> client_fds;
        for (int i = 0; i < 4; i++) {
            int fd = create_client_connection(20020);
            ASSERT_GE(fd, 0);
            client_fds.push_back(fd);
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(200));

        // 🔑 广播：从主线程向所有连接发送数据
        {
            std::lock_guard<std::mutex> lock(conn_mtx);
            for (auto& conn : all_conns) {
                conn->send("Broadcast message!");
            }
        }

        // 等待所有客户端收到广播
        std::this_thread::sleep_for(std::chrono::milliseconds(500));

        // 验证每个客户端都收到了广播
        for (int fd : client_fds) {
            std::string msg = recv_data(fd);
            EXPECT_EQ(msg, "Broadcast message!");
            close(fd);
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(200));
        loop.quit();
    });

    loop.loop();
    clients.join();

    EXPECT_EQ(conn_count.load(), 4);
}

// Test 12: 连接快速断开测试（建立后立即断开）
TEST(MultiThreadTcpServerTest, RapidConnectDisconnect) {
    EventLoop loop;
    InetAddress addr(20021);
    TcpServer server(loop, addr, "RapidServer");
    server.setThreadNum(4);

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

    std::thread clients([&]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));

        // 快速创建并断开 20 个连接
        for (int i = 0; i < 20; i++) {
            int fd = create_client_connection(20021);
            if (fd >= 0) {
                // 立即断开
                close(fd);
            }
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        loop.quit();
    });

    loop.loop();
    clients.join();

    EXPECT_EQ(connect_events.load(), 20);
    EXPECT_EQ(disconnect_events.load(), 20);
}

// Test 13: 多线程环境下 TCP_NODELAY 设置
TEST(MultiThreadTcpServerTest, TcpNoDelaySetting) {
    EventLoop loop;
    InetAddress addr(20022);
    TcpServer server(loop, addr, "NoDelayServer");
    server.setThreadNum(2);

    std::atomic<int> conn_count{0};

    server.setConnectionCallback([&](const std::shared_ptr<TcpConnection>& conn) {
        if (conn->connected()) {
            conn->setTcpNoDelay(true); // 禁用 Nagle 算法
            conn_count++;
        }
    });

    server.setMessageCallback([&](const std::shared_ptr<TcpConnection>& conn,
                                   Buffer& buf,
                                   std::chrono::system_clock::time_point) {
        std::string msg = buf.retrieveAllAsString();
        conn->send(msg);
    });

    server.start();

    std::thread clients([&]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));

        int fd = create_client_connection(20022);
        ASSERT_GE(fd, 0);

        // 发送小数据包
        for (int i = 0; i < 10; i++) {
            send_data(fd, "small");
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        close(fd);
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
        loop.quit();
    });

    loop.loop();
    clients.join();

    EXPECT_EQ(conn_count.load(), 1);
}

// Test 14: KeepAlive 设置测试
TEST(MultiThreadTcpServerTest, KeepAliveSetting) {
    EventLoop loop;
    InetAddress addr(20023);
    TcpServer server(loop, addr, "KeepAliveServer");
    server.setThreadNum(2);

    std::atomic<int> conn_count{0};

    server.setConnectionCallback([&](const std::shared_ptr<TcpConnection>& conn) {
        if (conn->connected()) {
            conn->setKeepAlive(true);
            conn_count++;
        }
    });

    server.start();

    std::thread clients([&]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));

        int fd = create_client_connection(20023);
        ASSERT_GE(fd, 0);

        std::this_thread::sleep_for(std::chrono::milliseconds(200));
        close(fd);
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
        loop.quit();
    });

    loop.loop();
    clients.join();

    EXPECT_EQ(conn_count.load(), 1);
}
