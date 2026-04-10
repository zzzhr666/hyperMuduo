#include <gtest/gtest.h>
#include <sys/socket.h>
#include <unistd.h>
#include <thread>
#include <chrono>
#include <atomic>
#include <string>
#include <vector>
#include <sstream>
#include <map>
#include "net/TcpServer.hpp"
#include "net/EventLoop.hpp"
#include "net/InetAddress.hpp"
#include "net/Buffer.hpp"
#include "net/TcpConnection.hpp"

using namespace hyperMuduo::net;

// ==================== 辅助函数（使用 static 避免链接冲突） ====================

static int create_client_connection(uint16_t port) {
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

static ssize_t send_data(int fd, const std::string& data) {
    return ::send(fd, data.data(), data.size(), 0);
}

static std::string recv_data(int fd, size_t max_bytes = 4096) {
    std::string buffer(max_bytes, 0);
    ssize_t n = ::recv(fd, buffer.data(), max_bytes, 0);
    if (n > 0) {
        buffer.resize(n);
        return buffer;
    }
    return "";
}

// ==================== 测试 1: 简单聊天服务器（广播模式） ====================
// 场景：多个客户端连接，任意客户端发送消息，服务器广播给所有连接的客户端

TEST(TcpServerAdvancedTest, ChatServerBroadcast) {
    EventLoop loop;
    InetAddress addr(21000);
    TcpServer server(loop, addr, "ChatServer");

    std::map<std::string, std::shared_ptr<TcpConnection>> clients;
    std::atomic<int> total_messages{0};

    server.setConnectionCallback([&](const std::shared_ptr<TcpConnection>& conn) {
        if (conn->connected()) {
            clients[conn->getName()] = conn;
        } else {
            clients.erase(conn->getName());
        }
    });

    server.setMessageCallback([&](const std::shared_ptr<TcpConnection>& sender,
                                   Buffer& buf,
                                   std::chrono::system_clock::time_point) {
        std::string msg = buf.retrieveAllAsString();
        total_messages++;
        for (auto& [name, client] : clients) {
            if (client->connected()) {
                client->send("[" + sender->getName() + "]: " + msg);
            }
        }
    });

    server.start();

    // 创建 2 个客户端
    int fd1 = -1, fd2 = -1;
    std::thread clients_thread([&]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));

        fd1 = create_client_connection(21000);
        fd2 = create_client_connection(21000);
        ASSERT_GE(fd1, 0);
        ASSERT_GE(fd2, 0);

        std::this_thread::sleep_for(std::chrono::milliseconds(50));

        // 两个客户端各发一条消息
        send_data(fd1, "Hello from 1");
        std::this_thread::sleep_for(std::chrono::milliseconds(50));

        send_data(fd2, "Hello from 2");
        std::this_thread::sleep_for(std::chrono::milliseconds(100));

        // 接收响应
        std::string resp1 = recv_data(fd1);
        std::string resp2 = recv_data(fd2);

        EXPECT_FALSE(resp1.empty());
        EXPECT_FALSE(resp2.empty());

        close(fd1);
        close(fd2);
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        loop.quit();
    });

    loop.loop();
    clients_thread.join();

    EXPECT_GE(total_messages.load(), 2);
}

// ==================== 测试 2: 请求-响应服务器（带会话状态） ====================
// 场景：客户端发送请求格式 "CMD:ARG"，服务器根据命令类型返回不同响应
// 使用 Context 存储每个连接的会话状态

struct SessionState {
    int request_count = 0;
    std::string last_command;
    std::chrono::steady_clock::time_point connect_time;
};

TEST(TcpServerAdvancedTest, RequestResponseWithSession) {
    EventLoop loop;
    InetAddress addr(21001);
    TcpServer server(loop, addr, "ReqRespServer");

    std::atomic<int> total_requests{0};
    std::map<std::string, SessionState> sessions;

    server.setConnectionCallback([&](const std::shared_ptr<TcpConnection>& conn) {
        if (conn->connected()) {
            SessionState state;
            state.connect_time = std::chrono::steady_clock::now();
            conn->setContext(std::make_any<SessionState>(state));
            sessions[conn->getName()] = state;
            conn->send("WELCOME\n");
        } else {
            sessions.erase(conn->getName());
        }
    });

    server.setMessageCallback([&](const std::shared_ptr<TcpConnection>& conn,
                                   Buffer& buf,
                                   std::chrono::system_clock::time_point) {
        std::string msg = buf.retrieveAllAsString();

        // 解析命令
        size_t colon_pos = msg.find(':');
        std::string cmd, arg;
        if (colon_pos != std::string::npos) {
            cmd = msg.substr(0, colon_pos);
            arg = msg.substr(colon_pos + 1);
        } else {
            cmd = msg;
            arg = "";
        }

        // 更新会话状态
        if (conn->hasContext<SessionState>()) {
            auto& state = conn->getContextAs<SessionState>();
            state.request_count++;
            state.last_command = cmd;
        }
        total_requests++;

        // 处理命令
        std::string response;
        if (cmd == "ECHO") {
            response = "ECHO:" + arg + "\n";
        } else if (cmd == "TIME") {
            auto now = std::chrono::system_clock::now();
            response = "TIME:" + std::to_string(
                std::chrono::duration_cast<std::chrono::seconds>(
                    now.time_since_epoch()).count()) + "\n";
        } else if (cmd == "STATUS") {
            if (conn->hasContext<SessionState>()) {
                auto& state = conn->getContextAs<SessionState>();
                response = "STATUS:requests=" + std::to_string(state.request_count) + "\n";
            }
        } else if (cmd == "QUIT") {
            response = "BYE\n";
            conn->send(response);
            conn->shutdown();
            return;
        } else {
            response = "ERROR:Unknown command\n";
        }

        conn->send(response);
    });

    server.start();

    std::thread client([&]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));

        int client_fd = create_client_connection(21001);
        ASSERT_GE(client_fd, 0);

        std::string response;

        // 接收欢迎消息
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        response = recv_data(client_fd);
        EXPECT_EQ(response, "WELCOME\n");

        // 发送 ECHO 命令
        send_data(client_fd, "ECHO:Hello Server");
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        response = recv_data(client_fd);
        EXPECT_TRUE(response.find("ECHO:Hello Server") != std::string::npos);

        // 发送 TIME 命令
        send_data(client_fd, "TIME:now");
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        response = recv_data(client_fd);
        EXPECT_TRUE(response.find("TIME:") == 0 || response.find("TIME:") != std::string::npos);

        // 发送 STATUS 命令
        send_data(client_fd, "STATUS:");
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        response = recv_data(client_fd);
        EXPECT_TRUE(response.find("STATUS:requests=3") != std::string::npos);

        // 发送错误命令
        send_data(client_fd, "INVALID");
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        response = recv_data(client_fd);
        EXPECT_TRUE(response.find("ERROR:Unknown command") != std::string::npos);

        // 发送 QUIT 命令
        send_data(client_fd, "QUIT:");
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        response = recv_data(client_fd);
        EXPECT_TRUE(response.find("BYE") != std::string::npos);

        close(client_fd);
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        loop.quit();
    });

    loop.loop();
    client.join();

    // 应该处理了 5 个请求（ECHO + TIME + STATUS + INVALID + QUIT）
    EXPECT_EQ(total_requests.load(), 5);
}

// ==================== 测试 3: 数据流服务器（大数据量传输） ====================
// 场景：客户端连接后，服务器发送大量数据块，验证数据完整性

TEST(TcpServerAdvancedTest, LargeDataStreaming) {
    EventLoop loop;
    InetAddress addr(21002);
    TcpServer server(loop, addr, "DataStreamer");

    constexpr size_t total_data_size = 100 * 1024; // 100 KB
    std::atomic<bool> transfer_complete{false};

    server.setMessageCallback([&](const std::shared_ptr<TcpConnection>& conn,
                                   Buffer&,
                                   std::chrono::system_clock::time_point) {
        // 收到任何消息后，开始流式传输数据
        std::string data(total_data_size, 0);
        for (size_t i = 0; i < total_data_size; i++) {
            data[i] = static_cast<char>(i % 256); // 填充模式数据
        }

        conn->send(data);
        transfer_complete = true;
    });

    server.start();

    std::thread client([&]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));

        int client_fd = create_client_connection(21002);
        ASSERT_GE(client_fd, 0);

        // 触发服务器开始传输
        send_data(client_fd, "START");

        // 接收所有数据
        std::string received_data;
        received_data.reserve(total_data_size);

        auto start_time = std::chrono::steady_clock::now();
        while (received_data.size() < total_data_size) {
            std::string chunk = recv_data(client_fd, 8192);
            if (chunk.empty()) {
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
                auto elapsed = std::chrono::steady_clock::now() - start_time;
                if (std::chrono::duration_cast<std::chrono::seconds>(elapsed).count() > 5) {
                    break; // 超时
                }
            } else {
                received_data += chunk;
            }
        }

        // 验证数据完整性
        EXPECT_EQ(received_data.size(), total_data_size);
        if (received_data.size() == total_data_size) {
            for (size_t i = 0; i < total_data_size; i++) {
                if (received_data[i] != static_cast<char>(i % 256)) {
                    FAIL() << "Data mismatch at offset " << i;
                    break;
                }
            }
        }

        close(client_fd);
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        loop.quit();
    });

    loop.loop();
    client.join();

    EXPECT_TRUE(transfer_complete.load());
}

// ==================== 测试 4: 大文件分块传输 ====================
// 场景：服务器发送大文件，客户端分块接收，验证数据完整性

TEST(TcpServerAdvancedTest, LargeFileTransfer) {
    EventLoop loop;
    InetAddress addr(21003);
    TcpServer server(loop, addr, "FileTransferServer");

    constexpr size_t file_size = 50 * 1024; // 50 KB
    std::atomic<bool> transfer_started{false};
    std::atomic<bool> transfer_completed{false};

    server.setMessageCallback([&](const std::shared_ptr<TcpConnection>& conn,
                                   Buffer& buf,
                                   std::chrono::system_clock::time_point) {
        std::string msg = buf.retrieveAllAsString();
        if (msg == "DOWNLOAD") {
            // 生成测试数据
            std::string data(file_size, 0);
            for (size_t i = 0; i < file_size; i++) {
                data[i] = static_cast<char>((i * 7 + 13) % 256);
            }

            transfer_started = true;
            conn->send(data);
            transfer_completed = true;
        }
    });

    server.start();

    std::thread client([&]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));

        int client_fd = create_client_connection(21003);
        ASSERT_GE(client_fd, 0);

        // 请求下载
        send_data(client_fd, "DOWNLOAD");

        // 接收文件数据
        std::string received_data;
        received_data.reserve(file_size);

        auto start_time = std::chrono::steady_clock::now();
        while (received_data.size() < file_size) {
            std::string chunk = recv_data(client_fd, 8192);
            if (chunk.empty()) {
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
                auto elapsed = std::chrono::steady_clock::now() - start_time;
                if (std::chrono::duration_cast<std::chrono::seconds>(elapsed).count() > 5) {
                    break; // 超时
                }
            } else {
                received_data += chunk;
            }
        }

        // 验证数据完整性
        EXPECT_EQ(received_data.size(), file_size);
        if (received_data.size() == file_size) {
            for (size_t i = 0; i < file_size; i++) {
                if (received_data[i] != static_cast<char>((i * 7 + 13) % 256)) {
                    FAIL() << "Data mismatch at offset " << i;
                    break;
                }
            }
        }

        close(client_fd);
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        loop.quit();
    });

    loop.loop();
    client.join();

    EXPECT_TRUE(transfer_started.load());
    EXPECT_TRUE(transfer_completed.load());
}

// ==================== 测试 5: 分块消息组装 ====================
// 场景：客户端分多次发送一条完整消息，服务器需要正确组装

TEST(TcpServerAdvancedTest, FragmentedMessageAssembly) {
    EventLoop loop;
    InetAddress addr(21004);
    TcpServer server(loop, addr, "FragmentServer");

    std::atomic<int> complete_messages{0};
    std::string full_message;

    server.setMessageCallback([&](const std::shared_ptr<TcpConnection>& conn,
                                   Buffer& buf,
                                   std::chrono::system_clock::time_point) {
        // 每次收到的可能是分片消息
        std::string fragment = buf.retrieveAllAsString();
        full_message += fragment;

        // 检查是否收到完整消息（以 END 结尾）
        if (full_message.size() >= 4 &&
            full_message.substr(full_message.size() - 3) == "END") {
            complete_messages++;
            conn->send("RECEIVED:" + std::to_string(full_message.size()) + "bytes");
            full_message.clear();
        }
    });

    server.start();

    std::thread client([&]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));

        int client_fd = create_client_connection(21004);
        ASSERT_GE(client_fd, 0);

        // 分 5 次发送一条消息
        std::vector<std::string> fragments = {
            "This ",
            "is a ",
            "fragment",
            "ed message ",
            "END"
        };

        size_t total_size = 0;
        for (const auto& frag : fragments) {
            total_size += frag.size();
            send_data(client_fd, frag);
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }

        // 接收确认
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        std::string response = recv_data(client_fd);
        EXPECT_EQ(response, "RECEIVED:" + std::to_string(total_size) + "bytes");

        close(client_fd);
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        loop.quit();
    });

    loop.loop();
    client.join();

    EXPECT_EQ(complete_messages.load(), 1);
}

// ==================== 测试 6: 连接数限制服务器 ====================
// 场景：服务器限制最大连接数为 3，超过限制的新连接会被拒绝

TEST(TcpServerAdvancedTest, ConnectionLimit) {
    EventLoop loop;
    InetAddress addr(21005);
    TcpServer server(loop, addr, "LimitServer");

    constexpr int max_connections = 3;
    std::atomic<int> accepted_connections{0};
    std::atomic<int> rejected_connections{0};
    std::map<std::string, std::shared_ptr<TcpConnection>> active_conns;

    server.setConnectionCallback([&](const std::shared_ptr<TcpConnection>& conn) {
        if (conn->connected()) {
            if (static_cast<int>(active_conns.size()) >= max_connections) {
                // 超过限制，拒绝连接
                rejected_connections++;
                conn->send("REJECTED:Server full\n");
                conn->shutdown();
            } else {
                accepted_connections++;
                active_conns[conn->getName()] = conn;
                conn->send("ACCEPTED\n");
            }
        } else {
            active_conns.erase(conn->getName());
        }
    });

    server.setMessageCallback([&](const std::shared_ptr<TcpConnection>& conn,
                                   Buffer&,
                                   std::chrono::system_clock::time_point) {
        conn->send("OK\n");
    });

    server.start();

    std::thread clients([&]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));

        std::vector<int> client_fds(5);
        std::vector<std::string> responses(5);

        // 创建 5 个连接（超过限制的 3 个）
        for (int i = 0; i < 5; i++) {
            client_fds[i] = create_client_connection(21005);
            std::this_thread::sleep_for(std::chrono::milliseconds(50));

            if (client_fds[i] >= 0) {
                responses[i] = recv_data(client_fds[i]);
            }
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(200));

        // 验证结果
        int accepted = 0, rejected = 0;
        for (int i = 0; i < 5; i++) {
            if (responses[i] == "ACCEPTED\n") {
                accepted++;
                close(client_fds[i]);
            } else if (responses[i] == "REJECTED:Server full\n") {
                rejected++;
                close(client_fds[i]);
            }
        }

        EXPECT_EQ(accepted, max_connections);
        EXPECT_EQ(rejected, 2);

        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        loop.quit();
    });

    loop.loop();
    clients.join();

    EXPECT_EQ(accepted_connections.load(), max_connections);
}
