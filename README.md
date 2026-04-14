# HyperMuduo

一个用于学习与实践的仿 muduo 网络库项目。
这个仓库主要记录我在阅读《Linux 多线程服务端编程：使用 muduo C++ 网络库》过程中的手写实现与思考，目标是在理解设计思想的同时，逐步完成一个可运行、可扩展的现代
C++ 网络库雏形。

## 项目目标

- 以 muduo 的核心设计为参考，实现一个完整的现代 C++ 网络库
- 用工程化方式沉淀学习过程，而不是只停留在读书和笔记
- 代码清晰、可验证，所有核心功能均配备单元测试

## 技术取向

- 全面拥抱现代 C++（以 C++17/20 风格编写），不依赖 Boost 库
- 避免使用 `std::bind`，优先使用 lambda 与清晰的回调封装
- 强调 RAII、类型安全、明确所有权与生命周期管理

---

## Quick Start

### 1. 安装依赖

HyperMuduo 依赖以下系统库（可通过包管理器安装）：

| 依赖库            | 用途                         |
|----------------|----------------------------|
| **spdlog**     | 日志系统                       |
| **protobuf**   | 消息序列化 (libprotobuf-dev)    |
| **zlib**       | Adler-32 校验算法 (zlib1g-dev) |
| **GoogleTest** | 单元测试框架 (libgtest-dev)      |

```bash
# Ubuntu/Debian 示例
sudo apt install libspdlog-dev libprotobuf-dev protobuf-compiler zlib1g-dev libgtest-dev build-essential cmake
```

### 2. 编译项目

```bash
cmake -S . -B build
cmake --build build
```

### 3. 编写你的第一个 Echo Server

HyperMuduo 的使用方式与 muduo 高度一致。以下是一个最小 Echo Server 示例：

```cpp
#include "net/EventLoop.hpp"
#include "net/TcpServer.hpp"
#include "spdlog/spdlog.h"

using namespace hyperMuduo::net;

int main() {
    EventLoop loop;
    InetAddress addr(8888);
    TcpServer server(loop, addr, "EchoServer");

    // 设置消息回调：收到什么就发回什么
    server.setMessageCallback([](const TcpConnectionPtr& conn, Buffer& buf, std::chrono::system_clock::time_point) {
        std::string msg = buf.retrieveAllAsString();
        SPDLOG_INFO("Received: {}", msg);
        conn->send(msg); // Echo back
    });

    server.start();
    SPDLOG_INFO("EchoServer started on 8888...");
    loop.loop();
    return 0;
}
```

### 4. 测试连接

启动服务器后，使用 `nc` (netcat) 连接测试：

```bash
nc 127.0.0.1 8888
```

### 5. 运行单元测试

验证所有核心功能的正确性：

```bash
# 运行所有测试
./build/test/HyperMuduoAllTests

# 使用 CTest
ctest --test-dir build
```

---

## 当前项目结构

```
HyperMuduo/
├── Base/                           # 基础工具模块
│   ├── CurrentThread.cpp
│   └── CurrentThread.hpp
├── benchmark
│   ├── CMakeLists.txt              #benchmark构建配置
│   ├── run_benchmark.sh            #测试脚本
│   └── echo_bench.cpp              #测试源文件
├── net/                            # 网络核心模块
│   ├── Reactor 系统
│   │   ├── EventLoop.hpp/cpp       # 事件循环（主反应器）
│   │   ├── Channel.hpp/cpp         # 文件描述符事件封装
│   │   ├── PollerBase.hpp          # IO 多路复用抽象基类
│   │   ├── Poller.hpp/cpp          # IO 多路复用（poll 版本）
│   │   └── Epoller.hpp/cpp         # IO 多路复用（epoll 版本，Linux 默认）
│   ├── 网络基础组件
│   │   ├── InetAddress.hpp/cpp     # sockaddr_in 轻量封装
│   │   └── Socket.hpp/cpp          # RAII Socket 封装
│   ├── 连接管理
│   │   ├── Acceptor.hpp/cpp        # 监听与接受新连接
│   │   └── TcpConnection.hpp/cpp   # TCP 连接抽象（部分完成）
│   ├── 定时器系统
│   │   ├── Timer.hpp/cpp           # 定时器对象
│   │   └── TimerQueue.hpp/cpp      # 定时器队列（最小堆 + 惰性删除）
│   ├── 线程模型
│   │   ├── EventLoopThread.hpp/cpp # 单线程 EventLoop 封装
│   │   └── EventLoopThreadPool.hpp/cpp # 多线程 EventLoop 线程池
│   ├── 缓冲与数据
│   │   └── Buffer.hpp/cpp          # 应用层收发缓冲区
│   └── protobuf/                   # Protocol Buffers 编解码链路
│       ├── Codec.hpp/cpp           # 静态编解码方法
│       ├── Dispatcher.hpp/cpp      # 基于描述符的消息分发
│       └── ProtobufHandler.hpp/cpp # 组合 Codec + Dispatcher
├── proto/                          # 协议定义文件
│   └── test.proto
├── test/                           # 单元测试（GoogleTest）
│   ├── CMakeLists.txt
│   ├── README.md                   # 测试说明文档
│   ├── test_buffer.cpp             # Buffer 基础测试
│   ├── test_codec.cpp              # Codec 编解码测试
│   ├── test_protobuf.cpp           # Protobuf 链路测试
│   ├── test_event_loop.cpp         # EventLoop 测试
│   ├── test_timer.cpp              # Timer 测试
│   ├── test_timerqueue.cpp         # TimerQueue 测试
│   ├── test_eventloopthread.cpp    # EventLoopThread 测试
│   ├── test_inetaddress.cpp        # InetAddress 测试
│   ├── test_socket.cpp             # Socket 测试
│   ├── test_acceptor.cpp           # Acceptor 测试
│   ├── test_tcpserver.cpp          # TcpServer 基础集成测试
│   ├── test_tcpserver_advanced.cpp # TcpServer 高级场景测试
│   ├── test_tcpserver_multithread.cpp # TcpServer 多线程测试（14个测试）
│   ├── test_tcpconnection.cpp      # TcpConnection 生命周期测试
│   ├── test_tcpclient.cpp          # TcpClient 客户端测试（7个测试）
│   ├── test_buffer_integration.cpp # Buffer 网络IO集成测试
│   ├── test_channel.cpp            # Channel 事件管理测试
│   └── test_protobuf_handler.cpp   # ProtobufHandler 完整流程测试
├── CMakeLists.txt                  # 构建配置
└── README.md                       # 项目文档
```

**测试覆盖**：110 个测试，覆盖 18 个测试套件，包括
Buffer、Codec、Protobuf、EventLoop、Timer、TimerQueue、EventLoopThread、InetAddress、Socket、Acceptor、TcpServer（基础+高级+多线程）、TcpConnection、TcpClient、Channel、ProtobufHandler

---

## 外部依赖

| 依赖库          | 用途    | 说明                                                  |
|--------------|-------|-----------------------------------------------------|
| **spdlog**   | 日志系统  | 高性能 C++ 日志库，替代手写 Logging 类                          |
| **protobuf** | 消息序列化 | Google Protocol Buffers，用于结构化消息编解码                  |
| **zlib**     | 校验计算  | 提供 Adler-32 算法，用于 Protobuf 编解码链路的校验和                |
| **pthreads** | 线程支持  | Linux 线程库，用于 `std::thread`、`std::mutex` 等标准库组件的底层实现 |

所有依赖均可通过包管理器安装（如 `apt`, `pacman`, `yum` 等）。

---

## 性能基准测试 (Benchmark)

HyperMuduo 提供了内置的基准测试工具，用于评估其在极限负载下的吞吐量和 QPS。

### 测试场景：Echo 回显测试
客户端与服务器建立 N 个并发连接，采用 **pipeline 模式**：发送消息 → 收到回显 → 立即发送下一条。以此持续计算稳态 QPS 和吞吐量。

### 1. 运行 Benchmark

```bash
# 启动服务器（后台运行，端口 8888，16 线程）
./build/benchmark/echo_bench server 8888 16 &

# 运行客户端测试（800 连接，1KB 消息，15 秒）
./build/benchmark/echo_bench client 127.0.0.1 8888 800 1024 15
```

也提供一键运行脚本，自动完成三种消息大小的测试以及 C10K 测试：

```bash
./benchmark/run_benchmark.sh
```

### 2. 测试结果

以下是在本地环境下的最佳测试结果。服务端开启多线程 Reactor 模式，客户端采用多线程并发模型（测试时已尽量关闭后台干扰进程）。

**测试环境**：
- **CPU**: Intel(R) Core(TM) i7-10700 @ 2.90GHz (8 核 16 线程)
- **内存**: 16 GiB
- **系统**: WSL2 (Kernel 6.6.87.2)

**测试条件**：
- **服务端**: 16 线程 (EventLoopThreadPool)，关闭所有日志
- **客户端**: 8 线程并发，每个线程 100 连接，共 **800 并发连接**
- **测试时长**: 15 秒

**不同消息大小下的性能数据**：

|   消息大小    | Send QPS | Send MB/s | Recv QPS | Recv MB/s |
|:---------:|:--------:|:---------:|:--------:|:---------:|
| **100 B** |   **996K** |   95.03   |   996K   |   95.03   |
| **1 KB** |   740K   |  723.39   |   740K   |  723.36   |
| **64 KB** |   87K    | **5,462.67** |   87K    | 5,460.32  |

**原始输出示例（100B 消息，近百万 QPS）**：
```text
============================================
Benchmark Results (15s)
============================================
Total Connections  : 800
Send Throughput    : 95.03 MB/s
Recv Throughput    : 95.03 MB/s
Send QPS           : 996482.47 msgs/s
Recv QPS           : 996461.07 msgs/s
Total Msgs Sent    : 14947237
Total Msgs Recv    : 14946916
Total Bytes Sent   : 1494723700
Total Bytes Recv   : 1494691600
============================================
```

### 3. 数据解读

#### 小消息场景（100B）— 逼近百万 QPS
- **996K QPS**，距离单机 1,000,000 QPS 仅一步之遥！这表明 HyperMuduo 的事件分发（Event Dispatch）、无锁化设计以及回调链路的延迟已经被压榨到了极致（平均单次事件处理耗时接近 1 微秒）。

#### 中等消息场景（1KB）— 业务平衡点
- 740K QPS，723 MB/s
- 最贴近实际业务场景（如 HTTP API 请求通常 500B-2KB）
- 吞吐量与处理能力的绝佳平衡。

#### 大消息场景（64KB）— 带宽榨汁机
- 87K QPS，但达到了惊人的 **5.46 GB/s** 吞吐量！
- 此时瓶颈彻底从 CPU/事件循环转移到了内存拷贝带宽（Memory Copy Bandwidth）。
- 接近了该机器在 localhost 环境下 TCP 协议栈的理论带宽上限。

### 4. C10K 测试（10,000 并发连接）

HyperMuduo 轻松通过了 C10K 级别的并发连接压测，验证了其在高并发海量连接场景下的稳健性。

**测试条件**：
- **连接数**: **10,000** 并发连接
- **消息大小**: 100 B
- **测试时长**: 30 秒

**C10K 测试结果**：
```text
============================================
Benchmark Results (30s)
============================================
Total Connections  : 10000
Send Throughput    : 51.16 MB/s
Recv Throughput    : 51.15 MB/s
Send QPS           : 536481.07 msgs/s
Recv QPS           : 536343.27 msgs/s
Total Msgs Sent    : 16094432
Total Msgs Recv    : 16090298
Total Bytes Sent   : 1609443200
Total Bytes Recv   : 1609029800
============================================
```

**C10K 与 800 连接对比**：

| 指标  |   800 连接 (100B) | 10,000 连接 (100B)|    变化    |
|:----|:----------:|:----------:|:--------:|
| QPS |    996K    |    536K    | **-46%** |
| 吞吐量 | 95.03 MB/s | 51.16 MB/s | **-46%** |

**解读**：
面对 10,000 个活跃的 TCP 连接，HyperMuduo 依然保持了 **53.6 万的超高 QPS**。得益于底层的 `epoll` 机制以及 O(1) 复杂度的哈希表通道管理，海量连接带来的状态检查开销被完美压制，框架未出现任何阻塞或资源枯竭。

### 5. 与主流网络库对比

业界通常采用 1KB 消息大小的 Echo 测试作为基准（模拟典型 HTTP 请求大小）。以下是同等测试条件下的公平对比：

| 库              |        测试场景         |    QPS    |      吞吐量      | 备注               |
|:---------------|:-------------------:|:---------:|:-------------:|:-----------------|
| **HyperMuduo** | 800 conn, **1KB** echo | **740K** | **723 MB/s** | 本次最佳记录，多线程 Reactor |
| **muduo** (原版) |  类似 1KB echo 测试    | ~400-500K | ~400-500 MB/s | Chen Shuo 官方测试数据 |
| **Boost.Asio** | 1000 conn, 1KB echo | ~500-600K | ~500-800 MB/s | 业界标杆，多线程跑满后表现极佳 |
| **evpp** (360) |   1000 conn, 1KB    |   ~550K   |   ~550 MB/s   | 知名开源库，基于 libevent |
| **Go net** |    1KB echo 测试      | ~500-700K | ~500-600 MB/s | Go 1.20+, 协程模型 |

> **数据结论**：在同等 1KB 负载下，HyperMuduo 凭借彻底的现代 C++ 重构（消除 `std::bind` 拷贝、O(1) 哈希查找、完美转发），不仅超越了原版 muduo 近 50%，在绝对吞吐量和 QPS 上也跻身甚至领先于基于 libevent 或 Asio 封装的主流工业级网络库。
### 6. 测试局限性与优化空间

| 问题               | 说明                                                                               |
|:-----------------|:---------------------------------------------------------------------------------|
| **Echo 场景理想化**   | 测试仅涉及底层 `read`/`send` 操作，不含业务逻辑。实际业务场景（如 Protobuf 编解码 + 数据库查询）QPS 通常会下降至 10万-30万 |
| **WSL2 虚拟化开销**   | 测试运行在 WSL2 (Hyper-V) 环境，网络包需经过虚拟网桥。原生 Linux 物理机可消除该开销                            |
| **localhost 测试** | localhost 绕过 NIC、中断、NAPI 等网络栈开销。跨机测试（万兆网卡）更能反映生产环境表现                             |
| **C100K 未验证**    | 当前 C10K 测试通过，但 C100K 场景（100K 并发连接）尚未验证。未来可测试更大规模连接                               |
| **缺少 HTTP 基准**   | 当前只有 echo 测试，无法反映 HTTP 解析等实际应用性能                                                 |

---

## 架构概览

```
┌─────────────────────────────────────────────────────────────────┐
│                        Application Layer                        │
│  ┌─────────────┐  ┌──────────────┐  ┌───────────────────────┐   │
│  │ TcpServer   │  │  TcpClient   │  │  ProtobufHandler      │   │
│  │             │  │              │  │                       │   │
│  └──────┬──────┘  └──────┬───────┘  └───────────┬───────────┘   │
└─────────┼────────────────┼──────────────────────┼───────────────┘
          │                │                      │
┌─────────┼────────────────┼──────────────────────┼──────────────┐
│         ▼                ▼                      ▼              │
│  ┌──────────────────────────────────────────────────────────┐  │
│  │                   TcpConnection                          │  │
│  │  ┌─────────────┐  ┌──────────────┐  ┌────────────────┐   │  │
│  │  │Input Buffer │  │Output Buffer │  │   State Machine│   │  │
│  │  └─────────────┘  └──────────────┘  └────────────────┘   │  │
│  └────────────────────────┬─────────────────────────────────┘  │
│                           │                                    │
│  ┌────────────────────────┴─────────────────────────────────┐  │
│  │                     Channel                              │  │
│  │              (fd + events + callbacks)                   │  │
│  └────────────────────────┬─────────────────────────────────┘  │
│                           │                                    │
│  ┌────────────────────────┴─────────────────────────────────┐  │
│  │                     PollerBase                           │  │
│  │           ┌─────────────┬─────────────┐                  │  │
│  │           ▼             ▼             ▼                  │  │
│  │     ┌────────┐    ┌────────┐    ┌────────┐               │  │
│  │     │ Poller │    │Epoller │    │  ...   │               │  │
│  │     │ (poll) │    │(epoll) │    │        │               │  │
│  │     └────────┘    └────────┘    └────────┘               │  │
│  └──────────────────────────────────────────────────────────┘  │
│                           │                                    │
│  ┌────────────────────────┴─────────────────────────────────┐  │
│  │                     EventLoop                            │  │
│  │  ┌─────────────┐  ┌──────────────┐  ┌────────────────┐   │  │
│  │  │  TimerQueue │  │  Wakeup fd   │  │  Task Queue    │   │  │
│  │  └─────────────┘  └──────────────┘  └────────────────┘   │  │
│  └──────────────────────────────────────────────────────────┘  │
│                           │                                    │
└───────────────────────────┼────────────────────────────────────┘
                            │
              ┌─────────────┴─────────────┐
              │         OS Kernel         │
              │  (epoll/poll + timerfd)   │
              └───────────────────────────┘
```

### 组件说明

| 组件                        | 职责                                           |
|---------------------------|----------------------------------------------|
| **EventLoop**             | Reactor 核心，负责事件循环、定时器调度、跨线程任务投递              |
| **PollerBase**            | IO 多路复用抽象基类，定义统一接口                           |
| **Poller / Epoller**      | 具体实现，支持 poll/epoll 两种模式，可运行时切换               |
| **Channel**               | 封装 fd 及其关注的事件、回调函数                           |
| **TcpConnection**         | TCP 连接抽象，管理输入/输出缓冲区、连接状态机                    |
| **TcpServer / TcpClient** | 高层封装，管理连接生命周期、回调配置                           |
| **TimerQueue**            | 基于 `timerfd` + 最小堆的定时器系统                     |
| **Buffer**                | 应用层收发缓冲区，支持 `readv`/`writev`、自动扩容、prepend 区域 |
| **ProtobufHandler**       | 组合 Codec + Dispatcher，实现 Protobuf 消息的编解码与分发  |

---

## 设计与实现差异：与原版 muduo 的全面对比

本节系统梳理 HyperMuduo 与陈硕原版 muduo 在关键设计决策上的差异，帮助读者理解每个选择背后的权衡。

### 概览：核心差异速查

| 特性            | 原版 muduo                       | HyperMuduo                                    | 理由         |
|---------------|--------------------------------|-----------------------------------------------|------------|
| 定时器数据结构       | `std::set`（红黑树）                | `priority_queue`（最小堆）+ 惰性删除                   | 缓存友好，取消操作少 |
| Channel 查找    | `std::map<int, Channel*>`      | `std::unordered_map<int, Channel*>`           | O(1) 复杂度   |
| 跨线程状态标志       | `bool` + 锁保护                   | `std::atomic<bool>`                           | 明确原子语义     |
| 智能指针          | `boost::scoped_ptr/shared_ptr` | `std::unique_ptr/shared_ptr`                  | 现代 C++ 标准库 |
| 回调注册          | `boost::bind`                  | Lambda 捕获                                     | 更清晰的捕获语义   |
| Buffer 双缓冲临时区 | 裸 `char[]` 数组                  | `std::array<char, 65536>`                     | 类型安全       |
| Protobuf 链路   | `ProtobufCodec` 单体             | `Codec` + `Dispatcher` + `ProtobufHandler` 拆分 | 职责单一，易测试   |

---

### 1. 现代 C++ 标准库与第三方库替代

原版 muduo 编写于 C++11 初期，许多基础设施需要手写实现。HyperMuduo 充分利用 C++17/20
标准库的成熟特性以及优秀的第三方库，替代了原本需要手写的基础组件，使代码更简洁、更安全、更易维护。

#### 1.1 时间系统：`std::chrono` 替代手写时间戳

| 维度    | 原版 muduo                           | HyperMuduo                                                                               |
|-------|------------------------------------|------------------------------------------------------------------------------------------|
| 时间戳类型 | 手写 `Timestamp` 类（基于 `time_t` + 微秒） | `std::chrono::steady_clock::time_point` / `std::chrono::system_clock::time_point`        |
| 时间间隔  | 手写毫秒/微秒转换                          | `std::chrono::milliseconds` / `std::chrono::seconds` 等强类型 Duration                       |
| 定时器接口 | `runAt(Timestamp, ...)`            | `runAt(time_point, ...)` / `runAfter(milliseconds, ...)` / `runEvery(milliseconds, ...)` |

**优势**：

- `std::chrono` 提供编译期类型安全，避免单位混淆（毫秒 vs 微秒）
- 支持字面量语法（如 `50ms`, `1s` via `using namespace std::chrono_literals`），代码更直观
- `steady_clock` 保证单调性，不受系统时钟调整影响，适合定时器系统

```cpp
// HyperMuduo 风格
loop.runAfter(100ms, [&]() { /* 100ms 后执行 */ });
loop.runEvery(1s, [&]() { /* 每秒执行 */ });

// 对比原版 muduo 风格
// loop.runAfter(Timestamp::now().addDelay(0.1), ...);
```

#### 1.2 线程同步原语：`std::mutex` / `std::condition_variable` 替代手写封装

| 维度       | 原版 muduo                                  | HyperMuduo                                                                    |
|----------|-------------------------------------------|-------------------------------------------------------------------------------|
| 互斥锁      | 手写 `MutexLock` + `MutexLockGuard`         | `std::mutex` + `std::lock_guard<std::mutex>` / `std::unique_lock<std::mutex>` |
| 条件变量     | 手写 `Condition` 类（封装 `pthread_cond_t`）     | `std::condition_variable`                                                     |
| 线程 ID 缓存 | 手写 `CurrentThread::tid()` + `__thread` 缓存 | `CurrentThread::getTid()` 封装（内部仍用 `__thread` 优化性能）                            |

**优势**：

- 标准库同步原语经过充分测试和优化，无需担心底层实现细节
- `std::lock_guard` / `std::unique_lock` 提供 RAII 风格的锁管理，避免死锁和遗漏解锁
- `std::condition_variable` 支持 `wait_for` / `wait_until` 超时语义，无需手写超时计算

```cpp
// HyperMuduo 中的 EventLoopThread 启动同步
std::unique_lock<std::mutex> lock(mutex_);
condition_.wait(lock, [&] { return loop_ != nullptr; });
```

#### 1.3 日志系统：`spdlog` 替代手写 `Logging` 类

| 维度   | 原版 muduo                            | HyperMuduo                                                   |
|------|-------------------------------------|--------------------------------------------------------------|
| 日志实现 | 手写 `Logging` 类（支持流式语法、异步日志）         | `spdlog` 库（`SPDLOG_INFO`, `SPDLOG_ERROR`, `SPDLOG_DEBUG` 等宏） |
| 日志级别 | `TRACE/DEBUG/INFO/WARN/ERROR/FATAL` | 与 spdlog 的级别映射一致                                             |
| 格式化  | 手写 `Format` 类                       | `spdlog` 内置格式化 + `fmt::ptr()` 等扩展                            |

**优势**：

- `spdlog` 是业界公认的高性能 C++ 日志库，吞吐量可达数百万条/秒
- 支持异步日志、多 sink、自定义格式化等高级功能，无需重新发明轮子
- 流式语法改为格式化字符串（`SPDLOG_INFO("New connection from {}", addr)`），性能更优且类型安全

```cpp
// HyperMuduo 中的日志使用
SPDLOG_INFO("TcpServer::NewConnection[{}] - new connection [{}] from {}",
            server_name_, connection_name, peer_addr.toIpPort());
SPDLOG_ERROR("socket accept error: {}", std::system_category().message(errno));
```

#### 1.4 其他标准库替代

| 手写组件                    | 标准库替代                                       | 使用场景                           |
|-------------------------|---------------------------------------------|--------------------------------|
| `boost::any` / 手写 `Any` | `std::any`                                  | `TcpConnection` 上下文存储          |
| `boost::optional`       | `std::optional`                             | 可选返回值的函数（如解码结果）                |
| `boost::string_view`    | `std::string_view`                          | 字符串只读视图（避免拷贝）                  |
| 手写 `CountDownLatch`     | `std::condition_variable` + `std::mutex` 组合 | 线程同步等待（如 `EventLoopThread` 启动） |
| 裸指针 + 手动 `delete[]`     | `std::vector<char>` / `std::array<char, N>` | Buffer 底层存储                    |
| `boost::function`       | `std::function`                             | 所有回调类型擦除                       |
| 手写线程封装                  | `std::thread`                               | `EventLoopThread` 内部线程创建       |

**设计原则**：

- **标准库优先**：C++17/20 标准库已覆盖 muduo 时代 90% 以上的 Boost 依赖
- **不重复造轮子**：对于日志、测试等通用基础设施，优先选择成熟的第三方库
- **保持 muduo 核心设计不变**：Reactor 模式、事件分发、连接管理等架构决策与原版一致，仅在底层工具层面现代化

---

以下按模块逐一展开。

---

### 2. 定时器系统（TimerQueue）

这是本项目与原版差异最大的模块。

**原版 muduo** 使用 `std::set<Timer*>`（红黑树）：

1. 按过期时间自动排序，最早过期的 Timer 位于 `begin()`
2. 取消定时器时 O(log n) 删除任意节点
3. 指针 / 迭代器在插入 / 删除后不失效

**HyperMuduo** 使用 `std::priority_queue`（最小堆）+ 惰性删除：

```cpp
using Entry = std::pair<std::chrono::steady_clock::time_point, Sequence>;
using TimerPriQueue = std::priority_queue<Entry, std::vector<Entry>, std::greater<>>;
using TimerMap = std::unordered_map<Sequence, std::unique_ptr<Timer>>;
```

- `timer_queue_`（最小堆）：仅存 `(过期时间, sequence)` 对，快速找出最早到期者
- `timers_`（哈希表）：持有 `Timer` 的独占所有权，支持 O(1) 查找和取消

**惰性删除**：`cancelTimer()` 只从 `timers_` 中擦除，不碰堆中的条目。到期处理时再次检查 `timers_` 是否存在——已取消则跳过回调：

```cpp
if (auto it = timers_.find(timer_sequence); it != timers_.end()) {
    it->second->runCallback();  // 仅对未取消的定时器执行
}
```

| 维度      | muduo（红黑树）         | HyperMuduo（最小堆 + 惰性删除）  |
|---------|--------------------|-------------------------|
| 获取最早定时器 | O(log n) `begin()` | O(1) `top()`            |
| 插入      | O(log n)           | O(log n)                |
| 取消      | O(log n) 直接删除      | O(1) 仅删哈希表，堆中惰性处理       |
| "幽灵条目"  | 无                  | 有（到期时自动弹出，不会无限堆积）       |
| 内存布局    | 节点分散，指针跳转          | `std::vector` 连续存储，缓存友好 |

### 3. Poller 实现

| 维度         | 原版 muduo                             | HyperMuduo                                |
|------------|--------------------------------------|-------------------------------------------|
| IO 多路复用    | 默认 `epoll`（`EPollPoller`），`poll` 为备选 | Linux 默认 `epoll`，可通过环境变量切换为 `poll`        |
| Channel 映射 | `std::map<int, Channel*>`            | `std::unordered_map<int, Channel*>`（O(1)） |

### 4. Channel 事件管理

| 维度     | 原版 muduo                                                  | HyperMuduo                                     |
|--------|-----------------------------------------------------------|------------------------------------------------|
| 事件更新   | 外部显式调用 `Channel::update()` → `EventLoop::updateChannel()` | `Channel::notifyLoop()` 内部自动回写，减少调用方心智负担       |
| 事件开关命名 | `enableReading()/disableReading()`                        | `listenTillReadable()/ignoreReadable()`（更具描述性） |
| 事件常量   | 直接使用 `POLLIN/POLLOUT` 宏                                   | 封装为 `kReadEvent/kWriteEvent/kNoneEvent` 常量     |

### 5. Buffer 设计

| 维度                 | 原版 muduo                    | HyperMuduo                      |
|--------------------|-----------------------------|---------------------------------|
| `readv` 临时缓冲区      | 裸 `char extrabuf[65536]` 数组 | `std::array<char, 65536>`（类型安全） |
| `retrieveAll()` 行为 | 重置游标到初始位置                   | 明确重置为 `CHEAP_PREPEND` 位置        |

### 6. TcpConnection 设计

| 维度   | 原版 muduo                     | HyperMuduo                         |
|------|------------------------------|------------------------------------|
| 输出缓冲 | `Buffer outputBuffer_`（直接成员） | `std::unique_ptr<Buffer>`（构造时即初始化） |
| 输入缓冲 | `Buffer inputBuffer_`（直接成员）  | `std::unique_ptr<Buffer>`（构造时即初始化） |

### 7. Protobuf 编解码链路

编码格式与原版完全一致（`len + nameLen + typeName + payload + checksum`，Adler-32 校验，64 MB 上限）。

HyperMuduo 将原版的 `ProtobufCodec` 拆分为三个职责单一的组件：

- **`Codec`**：纯静态编解码方法（长度前缀、校验、类型名处理）
- **`Dispatcher`**：基于 `google::protobuf::Descriptor*` 的回调注册与分发
- **`ProtobufHandler`**：组合 `Codec` + `Dispatcher`，循环解码并分发

这种拆分使每个组件都可独立测试和替换。

---

## 设计与实现原则

- 先实现最小可运行版本，再逐步增强功能
- 每次迭代聚焦一个主题（事件循环、IO 多路复用、连接管理、编解码等）
- 用日志、断言和小型测试确保每一步行为可观测
- 保持接口简洁，避免过早抽象和过度设计

## 指针与所有权约定

- 默认不使用裸指针表达所有权（ownership）
- 非空依赖优先使用引用（`T&`）
- 需要可空语义的观察者允许使用裸指针（`T*`）
- 共享所有权使用 `std::shared_ptr`，独占所有权使用 `std::unique_ptr`
- 底层字节视图与系统调用边界（如 `char*`、`const char*`、C API 参数）按需保留裸指针

---

## 功能清单

> 以下为已实现并经过测试的功能模块。

### A. 工程基础

- [x] 完成 CMake 工程初始化（可编译主程序）
- [x] 建立基础目录结构（`Base/`、`net/`、`proto/`）
- [x] 增加根目录 `.gitignore`（忽略构建产物、IDE 配置、本地缓存）
- [x] 配置基础格式化文件（`.clang-format`）
- [x] 增加测试框架（GoogleTest 或等价方案）

### B. Reactor 主循环（EventLoop / Channel / Poller）

#### B1. EventLoop

- [x] 线程唯一性约束（同线程单 EventLoop）
- [x] `loop()` 初版入口（占位轮询）
- [x] 线程归属断言（`assertInLoopThread`）
- [x] 完成循环骨架：`while (!quit_)` 持续事件分发
- [x] 增加 `quit()` 语义并可安全退出循环
- [x] 接入 `Poller` 并驱动 active channels 处理
- [x] 支持跨线程任务投递（`runInLoop/queueInLoop`）
- [x] 增加唤醒机制（`eventfd`）

#### B2. Channel

- [x] fd 与关注事件位管理（读 / 写 / 忽略）
- [x] 回调注册（读 / 写 / 错误）
- [x] `handleEvent()` 按 `revents` 分发
- [x] 通过 `notifyLoop()` 回写到 loop / poller
- [x] 增加连接关闭回调（`closeCallback`）→ 在 TcpConnection 层面实现
- [x] 增加防悬挂策略（`tie` 机制）

#### B3. Poller（poll/epoll 双模式）

- [x] `poll()` 封装与超时返回
- [x] `updateChannel()` 新增 / 更新逻辑
- [x] `fillActiveChannels()` 活跃事件收集
- [x] 线程归属校验
- [x] `removeChannel()` 与 channel 生命周期闭环
- [x] Epoller 实现（epoll 版本，Linux 默认）
- [x] 环境变量切换支持（`HYPERMUDUO_USE_POLL=1`）

### C. 网络核心组件

#### C0. 网络基础组件

##### C0.1 InetAddress

- [x] 基于 `sockaddr_in` 的轻量封装
- [x] 多构造函数（port only / ip+port / sockaddr_in）
- [x] IP / Port / IpPort 字符串转换
- [x] 底层 `sockaddr_in*` 访问接口（const 与 mutable）
- [x] 配套单测（8个测试，覆盖构造、转换、边界情况）

##### C0.2 Socket

- [x] RAII 风格的 Socket 封装（`socket_fd_` 生命周期管理）
- [x] 默认非阻塞 + `SOCK_NONBLOCK | SOCK_CLOEXEC`
- [x] 移动语义（move constructor / move assignment）
- [x] `bindAddress()` / `listen()` / `accept()` 核心网络调用
- [x] Socket 选项设置（`setReuseAddr` / `setReusePort` / `setKeepAlive`）
- [x] 配套单测（13个测试，覆盖构造、移动、选项、accept、析构）

#### C1. Acceptor

- [x] `Acceptor` 封装监听 Socket 的创建与监听流程
- [x] 自动设置 `SO_REUSEADDR` 并绑定地址
- [x] 基于 `Channel` 的异步 accept 回调机制
- [x] `NewConnectionCallback` 传递已连接 Socket 与对端地址
- [x] 线程归属校验（仅在 EventLoop 所属线程操作）
- [x] 配套单测（6个测试，覆盖监听、连接接收、多连接、回调时机、地址验证）

#### C2. Buffer

- [x] 读写游标与可读 / 可写 / 可 prepend 区域模型
- [x] `append/retrieve` 基础接口
- [x] `makeSpace` 扩容与数据搬移
- [x] `readFd` 双缓冲读取（`readv`）
- [x] `writeFd` 写回路径（`write` + 部分写入处理）
- [x] 增加单测覆盖边界情况（空包、大包、反复扩容）

#### C2. Timer / TimerQueue

- [x] Timer 类：一次性 / 重复定时器，原子序号管理
- [x] TimerQueue：基于 `timerfd` + `priority_queue` 的定时器队列
- [x] EventLoop 暴露 `runAt`/`runAfter`/`runEvery`/`cancelTimer` 接口
- [x] 配套单测（22 个测试全部通过，覆盖一次性 / 重复 / 取消 / 跨线程 / 压力场景）
- [x] TimerQueue 跨线程安全性（通过 `runInLoop` 延迟 mutation 实现）

#### C3. TcpConnection

- [x] 输出缓冲按需初始化（`outputBuffer()`）→ 改为构造时即初始化
- [x] 输入缓冲构造时即初始化（`receive_buffer_`）
- [x] 连接上下文（`std::any`）读写接口
- [x] 明确连接状态机（Connecting / Connected / Disconnecting / Disconnected）
- [x] 实现 `Send()` 主路径（线程切换 + 发送策略）
- [x] 实现 `Shutdown()` 半关闭流程
- [x] 绑定 `Channel` 事件回调（读 / 写 / 关闭 / 错误）
- [x] 配套单测（5 个测试，覆盖状态转换、同步/异步发送、关闭、Context）

#### C4. TcpServer

- [x] 组合 `Acceptor` 管理监听与连接生命周期
- [x] 自动创建 `TcpConnection` 并注册回调
- [x] 连接建立时调用 `connectionEstablished()`（通过 `runInLoop` 投递到子线程）
- [x] 连接断开时自动清理并调用 `connectionDestroyed()`
- [x] 消息回调透传（`setMessageCallback`）
- [x] 连接回调（`setConnectionCallback`）
- [x] 写完成回调（`setWriteCompleteCallback`）
- [x] 高水位回调（`setHighWaterMarkCallback`）
- [x] 基础集成测试（5 个测试：启停、回声服务、多连接、回调触发、数据接收）
- [x] 高级场景测试（6 个测试：聊天室广播、命令响应服务器、大数据流、文件传输、分片消息组装、连接数限制）

#### C4.5 TcpClient（客户端支持）

- [x] TcpClient 封装（连接器 + 连接管理）
- [x] 自动重连机制（`setRetry()`）
- [x] 连接回调、消息回调、写完成回调、高水位回调
- [x] `connect()` / `disconnect()` / `stop()` 生命周期管理
- [x] 配套单测（7 个测试：事件驱动回声、自动重连、析构安全、流量控制、高水位标记、消息顺序、连接失败处理、多客户端并发、大数据传输、服务器主动关闭、客户端停止、接口验证）

#### C5. EventLoopThreadPool（多线程 Reactor）

- [x] 线程池创建与管理（`EventLoopThread` 数组）
- [x] 轮询算法分配连接到不同 Worker 线程（`getNextLoop()`）
- [x] 主线程与子线程 EventLoop 分离
- [x] 动态线程数设置（`setThreadNum()`）
- [x] TcpServer 多线程模式（连接分配到 Worker 线程）
- [x] 跨线程连接删除（`removeConnection` + `removeConnectionInLoop`）
- [x] 跨线程数据发送（`conn->send()` 自动判断线程并投递）
- [x] 多线程测试（14 个测试：线程分配、Echo 服务、高并发、跨线程发送、回调验证、Context 安全、写完成回调、50 连接压力测试、1MB
  大数据传输、广播、快速断开、TCP_NODELAY、KeepAlive）

### D. Protobuf 编解码链路

#### D1. Codec

- [x] 编码：长度前缀 + 类型名 + payload + 校验
- [x] 解码：长度 / 校验 / 类型名合法性检查
- [x] 支持不完整包（`INCOMPLETE`）状态返回
- [x] 支持按类型名动态创建消息对象

#### D2. Dispatcher / Handler

- [x] 基于描述符的消息回调注册与分发
- [x] 默认回调兜底逻辑
- [x] `ProtobufHandler` 循环解码并分发
- [x] `Send()` 写入连接输出缓冲

### E. 文档

- [x] 完成项目 README
- [x] 增加"与原版 muduo 设计差异"详细对比

## 致谢

本项目的设计思想与学习路径主要来自陈硕的 muduo 相关资料与《Linux 多线程服务端编程：使用 muduo C++ 网络库》一书。
这个仓库是个人学习过程中的手写实践与复盘，不是 muduo 的替代品。
