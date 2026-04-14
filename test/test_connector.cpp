#include <gtest/gtest.h>
#include <sys/socket.h>
#include <unistd.h>
#include <thread>
#include <chrono>
#include <atomic>
#include <future>
#include <optional>
#include <spdlog/spdlog.h>
#include "net/Connector.hpp"
#include "net/EventLoop.hpp"
#include "net/InetAddress.hpp"
#include "net/TcpServer.hpp"
#include "net/Buffer.hpp"

using namespace hyperMuduo::net;

// Test 1: 基本连接成功 - Connector 连接到监听中的 TcpServer
TEST(ConnectorTest, ConnectToServerSuccessfully) {
    EventLoop* server_loop_ptr = nullptr;
    std::promise<void> server_ready_promise;
    auto server_ready_future = server_ready_promise.get_future();

    // 在独立线程中运行服务器
    std::thread server_thread([&]() {
        EventLoop server_loop;
        server_loop_ptr = &server_loop;
        
        InetAddress server_addr(21000);
        TcpServer server(server_loop, server_addr, "TestServer");

        // 设置服务器的消息回调：回声
        server.setMessageCallback([&](const std::shared_ptr<TcpConnection>& conn,
                                       Buffer& buf,
                                       std::chrono::system_clock::time_point) {
            std::string msg = buf.retrieveAllAsString();
            conn->send(msg);
        });

        server.start();
        server_ready_promise.set_value();
        
        server_loop.loop();
    });

    // 等待服务器启动
    server_ready_future.wait();
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // 创建 Connector 并连接
    EventLoop client_loop;
    InetAddress target_addr("127.0.0.1", 21000);
    auto connector = std::make_shared<Connector>(client_loop, target_addr);

    std::optional<Socket> received_socket;
    std::promise<void> connected_promise;
    auto connected_future = connected_promise.get_future();

    connector->setNewConnectionCallback([&](Socket socket) {
        received_socket = std::move(socket);
        connected_promise.set_value();
        client_loop.quit();
    });

    connector->start();

    // 启动客户端事件循环（在当前线程）
    client_loop.loop();

    // 等待连接建立
    auto status = connected_future.wait_for(std::chrono::seconds(2));
    EXPECT_EQ(status, std::future_status::ready);
    EXPECT_TRUE(received_socket.has_value());

    if (received_socket.has_value() && received_socket->valid()) {
        int sockfd = received_socket->getFd();

        // 验证连接有效：发送数据
        std::string test_msg = "Hello from Connector!";
        ssize_t sent = ::send(sockfd, test_msg.data(), test_msg.size(), 0);
        EXPECT_EQ(sent, test_msg.size());

        // 接收回声
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        std::string response(4096, 0);
        ssize_t received = ::recv(sockfd, response.data(), response.size(), 0);
        if (received > 0) {
            response.resize(received);
            EXPECT_EQ(response, test_msg);
        }

        // Socket 析构会自动关闭 fd
    }

    // 清理服务器
    if (server_loop_ptr) {
        server_loop_ptr->quit();
    }
    server_thread.join();
}

// Test 2: 连接到不存在的地址 - 验证错误处理
TEST(ConnectorTest, ConnectToNonExistentServer) {
    EventLoop loop;
    // 连接到一个没有服务器监听的地址
    InetAddress target_addr("127.0.0.1", 21999);
    auto connector = std::make_shared<Connector>(loop, target_addr);

    std::atomic<bool> success_callback_called{false};

    // 不应该触发连接成功回调
    connector->setNewConnectionCallback([&](Socket) {
        success_callback_called = true;
    });

    connector->start();

    // 运行一段时间后停止
    std::thread stopper([&]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        connector->stop();
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        loop.quit();
    });

    loop.loop();
    stopper.join();

    // 连接成功回调应该没有被调用
    EXPECT_FALSE(success_callback_called.load());
}

// Test 3: start/stop 生命周期测试
TEST(ConnectorTest, StartAndStopLifecycle) {
    EventLoop loop;
    InetAddress target_addr("127.0.0.1", 21001);
    auto connector = std::make_shared<Connector>(loop, target_addr);

    std::atomic<bool> callback_called{false};
    connector->setNewConnectionCallback([&](Socket) {
        callback_called = true;
    });

    // 启动后立即停止（在没有服务器的情况下）
    connector->start();

    std::thread stopper([&]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        connector->stop();
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        loop.quit();
    });

    loop.loop();
    stopper.join();

    // 回调不应该被调用
    EXPECT_FALSE(callback_called.load());
}

// Test 4: restart 重试测试 - 验证同一个 Connector 实例的状态重置
TEST(ConnectorTest, RestartAfterStop) {
    EventLoop* server_loop_ptr = nullptr;
    std::promise<void> server_ready_promise;
    auto server_ready_future = server_ready_promise.get_future();

    // 1. 启动背景服务器
    std::thread server_thread([&]() {
        EventLoop server_loop;
        server_loop_ptr = &server_loop;

        InetAddress server_addr(21002);
        TcpServer server(server_loop, server_addr, "RestartTestServer");
        server.start();
        server_ready_promise.set_value();
        server_loop.loop();
    });

    server_ready_future.wait();
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // 2. 客户端逻辑：同一个 Connector，连接两次
    EventLoop client_loop;
    InetAddress target_addr("127.0.0.1", 21002);
    auto connector = std::make_shared<Connector>(client_loop, target_addr);

    std::atomic<int> connect_count{0};

    // 注意这里我们通过值捕获了 connector 的 shared_ptr，确保它的生命周期
    connector->setNewConnectionCallback([&, connector](Socket socket) {
        connect_count++;

        if (connect_count == 1) {
            SPDLOG_INFO("Test 4: First connection successful, stopping and restarting...");
            // 第一次连接成功：
            // 1. 我们没有保存 socket 对象，所以它离开作用域时会立刻析构，向服务器发送 FIN。
            // 2. 停止当前连接器
            connector->stop();

            // 3. 利用 EventLoop 的定时器，在 100ms 后触发重启
            // （注意：如果你的 runAfter 接收的是 double 秒数，请改为 0.1）
            client_loop.runAfter(std::chrono::milliseconds(100), [&, connector]() {
                connector->restart();
            });
        } else if (connect_count == 2) {
            SPDLOG_INFO("Test 4: Second connection successful!");
            // 第二次连接成功，测试圆满结束
            client_loop.quit();
        }
    });

    // 启动并阻塞等待
    connector->start();
    client_loop.loop();

    // 验证确实成功连接了两次
    EXPECT_EQ(connect_count.load(), 2);

    // 清理服务器
    if (server_loop_ptr) {
        server_loop_ptr->quit();
    }
    server_thread.join();
}


// Test 5: 高并发连接测试 - 多个 Connector 在同一个 EventLoop 中同时发起连接
TEST(ConnectorTest, MultipleConnectionsToSameServer) {
    EventLoop* server_loop_ptr = nullptr;
    std::promise<void> server_ready_promise;
    auto server_ready_future = server_ready_promise.get_future();

    std::thread server_thread([&]() {
        EventLoop server_loop;
        server_loop_ptr = &server_loop;

        InetAddress server_addr(21003);
        TcpServer server(server_loop, server_addr, "MultiConnectServer");
        server.start();
        server_ready_promise.set_value();
        server_loop.loop();
    });

    server_ready_future.wait();
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // 客户端逻辑：单线程高并发
    EventLoop client_loop;
    InetAddress target_addr("127.0.0.1", 21003);

    constexpr int num_connections = 3;
    int success_count = 0; // 因为全在 client_loop 线程里跑，所以不需要 atomic

    // 必须把 connector 存起来，否则还没连上智能指针就析构了
    std::vector<std::shared_ptr<Connector>> connectors;
    std::vector<std::optional<Socket>> connected_sockets(num_connections);

    // 在同一个 loop 中，一口气发起 3 个异步连接
    for (int i = 0; i < num_connections; i++) {
        auto connector = std::make_shared<Connector>(client_loop, target_addr);

        connector->setNewConnectionCallback([&, i](Socket socket) {
            SPDLOG_INFO("Test 5: Connection {} established.", i);
            connected_sockets[i] = std::move(socket); // 保存起来，防止立刻断开
            success_count++;

            // 当 3 个连接全部成功时，退出事件循环
            if (success_count == num_connections) {
                client_loop.quit();
            }
        });

        connectors.push_back(connector);
        connector->start(); // 非阻塞启动
    }

    // 启动事件循环，Poller 将同时监听这 3 个 fd 的可写事件
    client_loop.loop();

    // 验证结果
    EXPECT_EQ(success_count, num_connections);

    // 验证所有 Socket 都有效
    for (int i = 0; i < num_connections; i++) {
        EXPECT_TRUE(connected_sockets[i].has_value());
        EXPECT_TRUE(connected_sockets[i]->valid());
    }

    // 清理服务器
    if (server_loop_ptr) {
        server_loop_ptr->quit();
    }
    server_thread.join();
}

// Test 6: 回调验证 - 确保回调接收到有效的 Socket
TEST(ConnectorTest, CallbackReceivesValidSocket) {
    EventLoop* server_loop_ptr = nullptr;
    std::promise<void> server_ready_promise;
    auto server_ready_future = server_ready_promise.get_future();

    std::thread server_thread([&]() {
        EventLoop server_loop;
        server_loop_ptr = &server_loop;
        
        InetAddress server_addr(21004);
        TcpServer server(server_loop, server_addr, "ValidFdTestServer");
        server.start();
        server_ready_promise.set_value();
        server_loop.loop();
    });

    server_ready_future.wait();
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    EventLoop client_loop;
    InetAddress target_addr("127.0.0.1", 21004);
    auto connector = std::make_shared<Connector>(client_loop, target_addr);

    std::optional<Socket> received_socket;
    std::promise<void> connected_promise;
    auto connected_future = connected_promise.get_future();

    connector->setNewConnectionCallback([&](Socket socket) {
        received_socket = std::move(socket);
        connected_promise.set_value();
        client_loop.quit();
    });

    connector->start();
    client_loop.loop();

    auto status = connected_future.wait_for(std::chrono::seconds(2));
    EXPECT_EQ(status, std::future_status::ready);
    EXPECT_TRUE(received_socket.has_value());

    if (received_socket.has_value()) {
        EXPECT_TRUE(received_socket->valid());

        // 验证 Socket 是一个有效的已连接 socket
        sockaddr_in local_addr;
        socklen_t addr_len = sizeof(local_addr);
        int ret = getsockname(received_socket->getFd(), reinterpret_cast<sockaddr*>(&local_addr), &addr_len);
        EXPECT_EQ(ret, 0);

        // Socket 析构会自动关闭 fd
    }

    // 清理
    if (server_loop_ptr) {
        server_loop_ptr->quit();
    }
    server_thread.join();
}
