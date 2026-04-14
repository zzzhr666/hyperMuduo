/**
 * 高性能 Echo Benchmark
 * 
 * 用法:
 *   ./echo_bench server [mode] [options]
 * 
 * 模式:
 *   server  - 启动 echo 服务器
 *   client  - 启动 benchmark 客户端
 * 
 * 服务器模式:
 *   ./echo_bench server <port> [threads]
 *   例: ./echo_bench server 8888 8
 * 
 * 客户端模式:
 *   ./echo_bench client <ip> <port> <connections> [message_size] [duration]
 *   例: ./echo_bench client 127.0.0.1 8888 800 1024 30
 * 
 * 设计:
 * - 客户端采用 pipeline 模式：发送 → 收到回显 → 再发送
 * - 每个连接持续循环发消息，避免 echo storm
 * - 统计真实的吞吐量和 QPS
 */

#include "net/EventLoop.hpp"
#include "net/TcpServer.hpp"
#include "net/TcpClient.hpp"
#include "spdlog/spdlog.h"

#include <atomic>
#include <chrono>
#include <csignal>
#include <iomanip>
#include <iostream>
#include <memory>
#include <string>
#include <vector>
#include <thread>
#include <sys/sysinfo.h>

using namespace hyperMuduo::net;

// ============================================================
// 服务器模式
// ============================================================

void run_server(int port, int thread_num) {
    EventLoop loop;
    InetAddress addr(port);
    TcpServer server(loop, addr, "EchoServer");

    server.setThreadNum(thread_num);

    // 极简 Echo 逻辑
    server.setMessageCallback([](const TcpConnectionPtr& conn, Buffer& buf, std::chrono::system_clock::time_point) {
        conn->send(buf.peek(), buf.readableBytes());
        buf.retrieveAll();
    });

    SPDLOG_INFO("EchoServer started on port {}, threads: {}", port, thread_num);
    
    server.start();
    loop.loop();
}

// ============================================================
// 客户端模式 - 统计数据
// ============================================================

struct Stats {
    std::atomic<int64_t> bytes_sent{0};
    std::atomic<int64_t> bytes_received{0};
    std::atomic<int64_t> messages_sent{0};
    std::atomic<int64_t> messages_received{0};
    std::atomic<int> connections_active{0};
    std::atomic<bool> test_finished{false};
};

static Stats g_stats;

// ============================================================
// 客户端模式 - 工作线程
// ============================================================

struct ClientConfig {
    std::string server_ip;
    int port;
    int connections_per_thread;
    size_t message_size;
    int duration_seconds;
    int thread_idx;
};

void worker_thread(const ClientConfig& config) {
    EventLoop loop;
    std::vector<std::unique_ptr<TcpClient>> clients;
    clients.reserve(config.connections_per_thread);

    std::string payload(config.message_size, 'H');
    auto start_time = std::chrono::steady_clock::now();

    for (int i = 0; i < config.connections_per_thread; ++i) {
        std::string name = "W" + std::to_string(config.thread_idx) + "-C" + std::to_string(i);
        auto client = std::make_unique<TcpClient>(loop, InetAddress(config.server_ip, config.port), name);

        client->setConnectionCallback([&](const TcpConnectionPtr& conn) {
            if (conn->connected()) {
                g_stats.connections_active.fetch_add(1);
                // 连接建立后立即发送第一条消息
                conn->send(payload);
                g_stats.bytes_sent += payload.size();
                g_stats.messages_sent += 1;
            }
        });

        client->setMessageCallback([&](const TcpConnectionPtr& conn, Buffer& buf, std::chrono::system_clock::time_point) {
            // 收到回显后，立即发送下一条消息 (pipeline)
            size_t bytes = buf.readableBytes();
            g_stats.bytes_received += bytes;
            g_stats.messages_received += 1;
            buf.retrieveAll();

            if (!g_stats.test_finished.load()) {
                conn->send(payload);
                g_stats.bytes_sent += payload.size();
                g_stats.messages_sent += 1;
            }
        });

        client->connect();
        clients.push_back(std::move(client));
    }

    // 定时退出
    loop.runAfter(std::chrono::seconds(config.duration_seconds + 2), [&]() {
        g_stats.test_finished.store(true);
        loop.quit();
    });

    loop.loop();
}

// ============================================================
// 客户端模式 - 统计打印
// ============================================================

void print_stats(int total_connections, int duration_seconds) {
    double duration_sec = duration_seconds;
    double sent_mbs = static_cast<double>(g_stats.bytes_sent.load()) / 1024.0 / 1024.0 / duration_sec;
    double recv_mbs = static_cast<double>(g_stats.bytes_received.load()) / 1024.0 / 1024.0 / duration_sec;
    double sent_qps = static_cast<double>(g_stats.messages_sent.load()) / duration_sec;
    double recv_qps = static_cast<double>(g_stats.messages_received.load()) / duration_sec;

    std::cout << std::fixed << std::setprecision(2);
    std::cout << "\n============================================\n";
    std::cout << "Benchmark Results (" << duration_seconds << "s)\n";
    std::cout << "============================================\n";
    std::cout << "Total Connections  : " << g_stats.connections_active.load() << "\n";
    std::cout << "Send Throughput    : " << sent_mbs << " MB/s\n";
    std::cout << "Recv Throughput    : " << recv_mbs << " MB/s\n";
    std::cout << "Send QPS           : " << sent_qps << " msgs/s\n";
    std::cout << "Recv QPS           : " << recv_qps << " msgs/s\n";
    std::cout << "Total Msgs Sent    : " << g_stats.messages_sent.load() << "\n";
    std::cout << "Total Msgs Recv    : " << g_stats.messages_received.load() << "\n";
    std::cout << "Total Bytes Sent   : " << g_stats.bytes_sent.load() << "\n";
    std::cout << "Total Bytes Recv   : " << g_stats.bytes_received.load() << "\n";
    std::cout << "============================================\n";
}

void run_client(const std::string& ip, int port, int total_connections, size_t message_size, int duration_seconds) {
    int thread_count = std::max(2, get_nprocs() / 2);
    int connections_per_thread = total_connections / thread_count;
    int actual_connections = connections_per_thread * thread_count;

    SPDLOG_DEBUG("========================================");
    SPDLOG_DEBUG(" Starting Echo Benchmark");
    SPDLOG_DEBUG("========================================");
    SPDLOG_DEBUG("Target: {}:{}", ip, port);
    SPDLOG_DEBUG("Threads: {}", thread_count);
    SPDLOG_DEBUG("Connections/Thread: {}", connections_per_thread);
    SPDLOG_DEBUG("Total Connections: {}", actual_connections);
    SPDLOG_DEBUG("Message Size: {} bytes", message_size);
    SPDLOG_DEBUG("Duration: {}s", duration_seconds);
    SPDLOG_DEBUG("========================================");

    std::vector<std::thread> workers;
    workers.reserve(thread_count);

    for (int i = 0; i < thread_count; ++i) {
        ClientConfig cfg{
            .server_ip = ip,
            .port = port,
            .connections_per_thread = connections_per_thread,
            .message_size = message_size,
            .duration_seconds = duration_seconds,
            .thread_idx = i
        };
        workers.emplace_back(worker_thread, std::cref(cfg));
    }

    for (auto& t : workers) {
        if (t.joinable()) {
            t.join();
        }
    }

    print_stats(actual_connections, duration_seconds);
}

// ============================================================
// 主函数
// ============================================================

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage:\n";
        std::cerr << "  Server: " << argv[0] << " server <port> [threads]\n";
        std::cerr << "  Client: " << argv[0] << " client <ip> <port> <connections> [message_size] [duration]\n";
        return 1;
    }

    std::string mode = argv[1];

    if (mode == "server") {
        if (argc < 3) {
            std::cerr << "Error: server mode requires <port>\n";
            return 1;
        }
        int port = std::atoi(argv[2]);
        int threads = (argc > 3) ? std::atoi(argv[3]) : get_nprocs();
        if (threads < 1) threads = 1;

        run_server(port, threads);

    } else if (mode == "client") {
        if (argc < 6) {
            std::cerr << "Error: client mode requires <ip> <port> <connections>\n";
            return 1;
        }
        std::string ip = argv[2];
        int port = std::atoi(argv[3]);
        int connections = std::atoi(argv[4]);
        size_t message_size = (argc > 5) ? std::stoul(argv[5]) : 1024;
        int duration = (argc > 6) ? std::atoi(argv[6]) : 30;

        run_client(ip, port, connections, message_size, duration);

    } else {
        std::cerr << "Error: unknown mode '" << mode << "'\n";
        std::cerr << "Valid modes: server, client\n";
        return 1;
    }

    return 0;
}
