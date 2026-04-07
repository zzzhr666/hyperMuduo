# HyperMuduo

一个用于学习与实践的仿 muduo 网络库项目。  
这个仓库主要记录我在阅读《Linux 多线程服务端编程：使用 muduo C++ 网络库》过程中的手写实现与思考，目标是在理解设计思想的同时，逐步完成一个可运行、可扩展的现代 C++ 网络库雏形。

## 项目目标

- 以 muduo 的核心设计为参考，按模块逐步实现网络库能力
- 用工程化方式沉淀学习过程，而不是只停留在读书和笔记
- 优先保证代码清晰、可验证、可迭代，便于后续持续重构

## 技术取向

- 全面拥抱现代 C++（以 C++17/20 风格编写）
- 尽量不依赖 Boost
- 避免使用 `std::bind`，优先使用 lambda 与清晰的回调封装
- 强调 RAII、类型安全、明确所有权与生命周期管理

## 当前项目结构（简要）

- `Base/`：基础工具模块（如线程相关）
- `net/`：网络核心模块（如 `EventLoop`、`Channel`、`Poller`、`TcpConnection`、`Buffer`）
- `net/protobuf/`：协议编解码与分发相关组件
- `proto/`：示例协议定义
- `main.cpp`：实验/入口代码

## 学习与实现原则

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

## TODO / 进度清单（持续维护）

> 说明：  
> - `[x]` 已完成；`[ ]` 未完成。  
> - 我们后续按这个清单持续更新，你每次实现完一个点都可以让我同步勾选。  
> - 当前状态已按仓库代码（`EventLoop/Channel/Poller/Buffer/TcpConnection/protobuf`）对齐。

### A. 工程基础

- [x] 完成 CMake 工程初始化（可编译主程序）
- [x] 建立基础目录结构（`Base/`、`net/`、`proto/`）
- [x] 增加根目录 `.gitignore`（忽略构建产物、IDE 配置、本地缓存）
- [x] 配置基础格式化文件（`.clang-format`）
- [x] 增加测试框架（GoogleTest 或等价方案）
- [ ] 增加 CI（至少包含构建检查）
- [ ] 增加静态检查（`clang-tidy`/`cppcheck`）

### B. Reactor 主循环（EventLoop / Channel / Poller）

#### B1. EventLoop

- [x] 线程唯一性约束（同线程单 EventLoop）
- [x] `loop()` 初版入口（占位轮询）
- [x] 线程归属断言（`assertInLoopThread`）
- [x] 完成循环骨架：`while (!quit_)` 持续事件分发
- [x] 增加 `quit()` 语义并可安全退出循环
- [x] 接入 `Poller` 并驱动 active channels 处理
- [x] 支持跨线程任务投递（`runInLoop/queueInLoop`）
- [x] 增加唤醒机制（`eventfd` 或 `pipe`）

#### B2. Channel

- [x] fd 与关注事件位管理（读/写/忽略）
- [x] 回调注册（读/写/错误）
- [x] `handleEvent()` 按 `revents` 分发
- [x] 通过 `update()` 回写到 loop/poller
- [ ] 增加连接关闭回调（`closeCallback`）
- [ ] 增加防悬挂策略（`tie` 机制）
- [ ] 统一接口命名（如 `enableReading/disableReading` 风格）

#### B3. Poller（poll 版本）

- [x] `poll()` 封装与超时返回
- [x] `updateChannel()` 新增/更新逻辑
- [x] `fillActiveChannels()` 活跃事件收集
- [x] 线程归属校验
- [x] `removeChannel()` 与 channel 生命周期闭环
- [ ] 优化 `fd < 0` 的禁用项处理和映射一致性
- [ ] 增加参数与状态合法性断言（索引/映射一致）
- [ ] 预留后续 `epoll` 版本抽象

### C. 网络核心组件

#### C1. Buffer

- [x] 读写游标与可读/可写/可 prepend 区域模型
- [x] `append/retrieve` 基础接口
- [x] `makeSpace` 扩容与数据搬移
- [x] `readFd` 双缓冲读取（`readv`）
- [ ] 增加 writeFd/send 路径
- [x] 增加单测覆盖边界情况（空包、大包、反复扩容）

#### B4. Timer / TimerQueue

- [x] Timer 类：一次性/重复定时器，原子序号管理
- [x] TimerQueue：基于 `timerfd` + `priority_queue` 的定时器队列
- [x] EventLoop 暴露 `runAt`/`runAfter`/`runEvery`/`cancelTimer` 接口
- [x] 配套单测（22 个测试全部通过，覆盖一次性/重复/取消/跨线程/压力场景）
- [ ] TimerQueue 跨线程安全性进一步完善

#### C2. TcpConnection

- [x] 输出缓冲按需初始化（`outputBuffer()`）
- [x] 连接上下文（`std::any`）读写接口
- [ ] 明确连接状态机（Connecting/Connected/Disconnecting/Disconnected）
- [ ] 实现 `Send()` 主路径（线程切换 + 发送策略）
- [ ] 实现 `Shutdown()` 半关闭流程
- [ ] 绑定 `Channel` 事件回调（读/写/关闭/错误）

### D. Protobuf 编解码链路

#### D1. Codec

- [x] 编码：长度前缀 + 类型名 + payload + 校验
- [x] 解码：长度/校验/类型名合法性检查
- [x] 支持不完整包（`INCOMPLETE`）状态返回
- [x] 支持按类型名动态创建消息对象
- [ ] 增加解码错误分级后的连接处理策略（丢包/断连）
- [ ] 覆盖粘包/拆包/畸形包的系统化测试

#### D2. Dispatcher / Handler

- [x] 基于描述符的消息回调注册与分发
- [x] 默认回调兜底逻辑
- [x] `ProtobufHandler` 循环解码并分发
- [x] `Send()` 写入连接输出缓冲
- [ ] 对接真实网络读写路径（从 `TcpConnection` 收发）
- [ ] 增加端到端示例（Echo 或 RPC 风格最小 demo）

### E. 文档与学习映射

- [x] 完成项目 README 初版
- [ ] 增加架构说明（线程模型、Reactor 流程图）
- [ ] 增加“书籍章节 -> 代码模块”映射表
- [ ] 增加设计决策记录（为什么这样设计）
- [ ] 增加常见问题与踩坑记录

## 近期计划（建议）

1. 先把 `EventLoop + Poller + Channel` 跑成稳定闭环
2. 做一个最小 Echo Server，打通连接、收发、回调路径
3. 逐步引入线程池/定时器等能力，并配套测试

## 致谢

本项目的设计思想与学习路径主要来自陈硕的 muduo 相关资料与《Linux 多线程服务端编程》一书。  
这个仓库是个人学习过程中的手写实践与复盘，不是 muduo 的替代品。
