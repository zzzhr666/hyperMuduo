# HyperMuduo 测试说明

本目录包含 HyperMuduo 项目的所有单元测试，使用 GoogleTest 框架编写。

## 快速开始

```bash
# 编译所有测试
cmake --build cmake-build-debug

# 运行单个测试
./cmake-build-debug/test/TcpServerTests

# 运行所有测试
./cmake-build-debug/test/HyperMuduoAllTests
```

## 测试清单

### 基础组件测试

#### `test_buffer.cpp` (2 个测试)
测试 Buffer 类的基础读写功能。
- **BasicReadWrite**: 验证 append/retrieve 基本流程
- **PeekAndRetrieve**: 验证 peek 指针和 retrieve 偏移

#### `test_codec.cpp` (1 个测试)
测试 Protobuf Codec 的编解码循环。
- **RoundTrip**: 编码 Query 消息后解码，验证字段完整性

#### `test_protobuf.cpp` (1 个测试)
测试 ProtobufHandler 的消息分发。
- **HandlerDispatch**: 注册 Query 回调，验证消息正确分发到类型回调

#### `test_event_loop.cpp` (4 个测试)
测试 EventLoop 的事件分发和线程模型。
- **SingleChannelDispatch**: 单个 Channel 的可读事件分发
- **MultiChannelDispatch**: 多个 Channel 同时事件分发
- **QuitFromOtherThread**: 从其他线程安全退出 EventLoop
- **ThreadAffinityFlags**: 验证 isInLoopThread() 线程归属判断

#### `test_timer.cpp` (3 个测试)
测试 EventLoop 暴露的定时器接口。
- **OneShotTimer**: 一次性定时器到期触发
- **RepeatAndCancel**: 重复定时器运行与取消
- **CrossThreadAdd**: 跨线程添加定时器

#### `test_timerqueue.cpp` (11 个测试)
测试 TimerQueue 的内部实现。
- 覆盖一次性/重复定时器、取消、多定时器排序、压力测试（100个定时器）、跨线程安全等场景

#### `test_eventloopthread.cpp` (6 个测试)
测试 EventLoopThread 的线程创建与生命周期。
- 验证 startLoop/quit、初始化回调、跨线程任务投递、析构清理等

#### `test_inetaddress.cpp` (8 个测试)
测试 InetAddress 的地址封装与转换。
- 覆盖 port only、ip+port、localhost、sockaddr_in 构造、端口范围等

#### `test_socket.cpp` (13 个测试)
测试 Socket 的 RAII 封装和网络操作。
- 覆盖构造/移动语义、bind/listen/accept、socket 选项（reuse addr/port, keepalive）、accept 非阻塞等

#### `test_acceptor.cpp` (6 个测试)
测试 Acceptor 的监听和连接接受。
- 覆盖监听启动、连接接收、多连接、回调时机验证、IP 绑定、对端地址接收

---

### 集成与高级测试（今日新增）

#### `test_tcpserver.cpp` (5 个测试)
测试 TcpServer 的基础集成流程。
- **StartAndStop**: 服务器启动和停止的生命周期
- **EchoServerSingleConnection**: 单连接回声服务器（收到什么发回什么）
- **MultipleConcurrentConnections**: 3 个客户端同时连接并发
- **ConnectionCallbackTriggers**: 验证连接建立/断开回调触发
- **MessageCallbackReceivesData**: 验证消息回调完整接收客户端数据

#### `test_tcpserver_advanced.cpp` (6 个测试)
构建复杂服务器应用场景，全面测试 TcpServer 的功能。
- **ChatServerBroadcast**: 多人聊天室广播模式（2 个客户端，消息广播给所有人）
- **RequestResponseWithSession**: 命令响应服务器（ECHO/TIME/STATUS/QUIT 命令，使用 Context 存储会话状态）
- **LargeDataStreaming**: 100KB 数据流式传输，验证大数据发送/接收
- **LargeFileTransfer**: 50KB 文件传输，逐字节校验数据完整性
- **FragmentedMessageAssembly**: 客户端分 5 次发送消息，服务器正确组装完整消息
- **ConnectionLimit**: 连接数限制服务器（最多 3 个连接，超过拒绝）

#### `test_tcpconnection.cpp` (5 个测试)
测试 TcpConnection 的连接生命周期。
- **ConnectionEstablishedStateTransition**: 验证状态从 Connecting → Connected 转换
- **SendDataInSameThread**: 同线程同步发送数据
- **SendDataFromOtherThread**: 跨线程异步发送数据（验证线程安全）
- **GracefulShutdown**: 优雅关闭连接，验证状态转换
- **ContextStorage**: 验证 std::any 上下文存储（存储/读取/修改/类型检查）

#### `test_buffer_integration.cpp` (7 个测试)
测试 Buffer 在实际网络 IO 场景中的表现。
- **ReadWriteThroughPipe**: 通过 pipe 测试 readFd 和 append
- **LargeDataTransfer**: 超过初始 buffer 大小时的自动扩容
- **PrependForProtocolHeader**: 模拟协议头部插入（prepend 功能）
- **SwapZeroCopy**: 两个 Buffer 间的零拷贝数据转移
- **BufferStateQueries**: 验证可读/可写/可prepend字节数查询
- **EnsureWritableBytesAutoExpand**: 验证自动扩容机制
- **HasWrittenManualUpdate**: 手动更新写指针功能

#### `test_channel.cpp` (6 个测试)
测试 Channel 的事件分发机制。
- **ReadableEventAndCallback**: 可读事件监听和回调触发
- **WritableEventAndCallback**: 可写事件监听和回调触发
- **IgnoreEvents**: 验证 ignoreAll() 后事件不再触发回调
- **ComprehensiveStateQueries**: 综合验证事件状态查询接口
- **TieMechanismPlaceholder**: tie 机制占位（已在 TcpConnection 中间接验证）
- **ErrorCallbackSetup**: 错误回调设置验证

#### `test_tcpclient.cpp` (7 个测试)
测试 TcpClient 客户端的完整功能与复杂场景。
- **HighWaterMarkTrigger**: 高水位标记回调测试（验证 API 配置与触发机制）
- **MessageOrdering**: 消息顺序性测试（使用长度前缀协议保证消息边界）
- **ConnectionFailureHandling**: 连接失败处理测试（连接不存在服务器时的错误处理）
- **MultipleConcurrentClients**: 多客户端并发连接测试（5 个客户端同时连接与通信）
- **LargeDataTransferIntegrity**: 大数据传输完整性测试（50KB 数据发送/接收/校验）
- **ServerInitiatedClose**: 服务器主动关闭连接测试（验证客户端断开处理）
- **StopClientTest**: 客户端停止功能测试（验证 stop() 方法安全性）

#### `test_protobuf_handler.cpp` (5 个测试)
测试 ProtobufHandler 的完整编解码流程。
- **EncodeDecodeQueryMessage**: 编码/解码 Query 消息循环
- **MultiMessageTypeDispatch**: 同时注册 Query 和 Answer 回调，验证正确分发
- **DefaultCallbackForUnregisteredMessage**: 未注册消息类型触发默认回调
- **ConsecutiveMessageProcessing**: Buffer 中有多条消息时的连续处理
- **SendMessageThroughConnection**: 通过 TcpConnection 发送 Protobuf 消息

#### `test_tcpserver_multithread.cpp` (14 个测试)
测试多线程 TcpServer 的完整功能与线程安全性。
- **ConnectionsDistributedToWorkers**: 验证连接被轮询分配到不同 Worker 线程
- **MultiThreadEchoServer**: 4 线程 Echo 服务验证
- **HighConcurrencyConnections**: 10 连接高并发与优雅断开
- **CrossThreadSend**: 主线程向子线程连接主动推送数据
- **CallbacksRunInWorkerThreads**: 验证所有 I/O 回调在子线程执行（非主线程）
- **ConnectionContextThreadSafety**: 连接上下文（`std::any`）多线程安全
- **WriteCompleteCallbackTriggers**: 写完成回调触发验证
- **StressTestManyConnections**: 50 连接压力测试
- **LargeDataTransfer**: 1MB 大数据分块传输
- **DynamicThreadNum**: 动态修改线程数（运行时扩容）
- **BroadcastToAllConnections**: 从主线程向所有连接广播消息
- **RapidConnectDisconnect**: 20 连接快速建立并断开
- **TcpNoDelaySetting**: TCP_NODELAY 设置验证
- **KeepAliveSetting**: KeepAlive 设置验证

---

## 测试统计

| 类别 | 文件数 | 测试用例数 |
|------|--------|-----------|
| 基础组件 | 11 | 55 |
| 集成与高级 | 7 | 55 |
| **总计** | **18** | **110** |

所有测试均通过，覆盖从基础组件到复杂服务器场景、从单线程到多线程、从服务端到客户端的完整功能链路。

## 已知 Bug 修复记录

在编写测试过程中发现并修复了以下隐藏 Bug：

1. **TcpServer 忘记调用 connectionEstablished()**
   → 导致连接状态永远停留在 Connecting，无法发送数据

2. **sendInLoop 访问未初始化的 send_buffer_ 空指针**
   → 首次发送数据时 Segmentation Fault（后改为构造时即初始化解决）

3. **connectionDestroyed() 未检查 connection_callback_ 是否为空**
   → 未设置连接回调时，连接销毁会崩溃

4. **多线程模式下 TcpConnection 构造函数断言失败**
   → TcpConnection 对象在主线程创建，但构造函数调用 `assertInLoopThread()` 检查子线程
   → 修复：移除构造函数断言，将 Channel 注册延迟到 `connectionEstablished()`（在子线程执行）

5. **跨线程删除连接时任务投递到错误的 Loop**
   → `removeConnectionInLoop` 中使用 `loop_.queueInLoop`（主线程）执行 `connectionDestroyed()`
   → 修复：改为 `conn->getLoop().queueInLoop()`，确保清理任务投递回子线程

6. **高水位回调从未被触发**
   → 设置了 `high_water_mark_` 但代码中从未检查并触发回调
   → 修复：在 `sendInLoop()` 中添加高水位检查逻辑

7. **TcpServer 的写完成回调未传递给 TcpConnection**
   → `setWriteCompleteCallback()` 设置了但没传给子连接
   → 修复：在 TcpServer 新连接回调中添加 `conn->setWriteCompleteCallback()`
