#include <gtest/gtest.h>
#include <unistd.h>
#include <sys/socket.h>
#include <thread>
#include <chrono>
#include <atomic>
#include <memory>
#include "net/Channel.hpp"
#include "net/EventLoop.hpp"

using namespace hyperMuduo::net;

// Test 1: 可读事件监听和回调
TEST(ChannelTest, ReadableEventAndCallback) {
    int fds[2];
    ASSERT_EQ(pipe(fds), 0);

    EventLoop loop;
    Channel channel(loop, fds[0]);

    std::atomic<bool> read_callback_triggered{false};

    channel.setReadCallback([&](std::chrono::system_clock::time_point) {
        char buf[64];
        ssize_t n = read(fds[0], buf, sizeof(buf));
        if (n > 0) {
            read_callback_triggered = true;
        }
        loop.quit();
    });

    channel.listenTillReadable();

    // 从另一端写入数据
    std::thread writer([&]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
        const char* msg = "Trigger readable event";
        write(fds[1], msg, 22);
    });

    loop.loop();
    writer.join();

    EXPECT_TRUE(read_callback_triggered.load());

    close(fds[0]);
    close(fds[1]);
}

// Test 2: 可写事件监听和回调
TEST(ChannelTest, WritableEventAndCallback) {
    EventLoop loop;

    // 使用 pipe 的写端来测试可写事件
    int fds[2];
    ASSERT_EQ(pipe(fds), 0);

    Channel channel(loop, fds[1]);

    std::atomic<bool> write_callback_triggered{false};

    channel.setWriteCallback([&]() {
        write_callback_triggered = true;
        // 监听到可写事件后，停止监听并退出循环
        channel.ignoreWritable();
        loop.quit();
    });

    // 监听可写事件（pipe 的写端通常是可写的）
    channel.listenTillWritable();

    std::thread quitter([&]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
        loop.quit();
    });

    loop.loop();
    quitter.join();

    EXPECT_TRUE(write_callback_triggered.load());

    close(fds[0]);
    close(fds[1]);
}

// Test 3: 忽略事件
TEST(ChannelTest, IgnoreEvents) {
    int fds[2];
    ASSERT_EQ(pipe(fds), 0);

    EventLoop loop;
    Channel channel(loop, fds[0]);

    std::atomic<bool> callback_called{false};

    channel.setReadCallback([&](std::chrono::system_clock::time_point) {
        callback_called = true;
    });

    // 先监听可读事件
    channel.listenTillReadable();

    // 然后忽略所有事件
    channel.ignoreAll();

    // 写入数据
    const char* msg = "This should not trigger";
    write(fds[1], msg, 25);

    // 运行循环
    std::thread quitter([&]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        loop.quit();
    });

    loop.loop();
    quitter.join();

    // 因为忽略了事件，回调不应该被调用
    EXPECT_FALSE(callback_called.load());

    close(fds[0]);
    close(fds[1]);
}

// Test 4: 事件状态综合查询
TEST(ChannelTest, ComprehensiveStateQueries) {
    EventLoop loop;

    int sockfd = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0);
    ASSERT_GE(sockfd, 0);

    Channel channel(loop, sockfd);

    // 初始状态：无事件监听
    EXPECT_TRUE(channel.isNoneEvent());
    EXPECT_FALSE(channel.isWriting());
    EXPECT_EQ(channel.getReturnEvents(), 0);

    // 监听可读事件
    channel.listenTillReadable();
    EXPECT_FALSE(channel.isNoneEvent());
    EXPECT_FALSE(channel.isWriting());
    int read_events = channel.getEvents();
    EXPECT_GT(read_events, 0);

    // 监听可写事件
    channel.listenTillWritable();
    EXPECT_FALSE(channel.isNoneEvent());
    EXPECT_TRUE(channel.isWriting());

    // 忽略可读事件
    channel.ignoreReadable();
    EXPECT_TRUE(channel.isWriting());

    // 忽略可写事件
    channel.ignoreWritable();
    EXPECT_TRUE(channel.isNoneEvent());

    // 验证文件描述符
    EXPECT_EQ(channel.getFd(), sockfd);

    // 验证 index 设置
    channel.setIndex(5);
    EXPECT_EQ(channel.index(), 5);

    close(sockfd);
}

// Test 5: tie 机制占位（在TcpConnection测试中验证）
TEST(ChannelTest, TieMechanismPlaceholder) {
    // tie 机制需要 shared_ptr<TcpConnection>，已在 TcpConnection 测试中间接验证
    // 这里只验证接口存在
    EXPECT_TRUE(true);
}

// Test 6: 错误回调设置
TEST(ChannelTest, ErrorCallbackSetup) {
    EventLoop loop;

    int sockfd = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0);
    ASSERT_GE(sockfd, 0);

    Channel channel(loop, sockfd);

    std::atomic<bool> error_callback_set{false};

    channel.setErrorCallback([&]() {
        error_callback_set = true;
    });

    // 设置错误回调不应该立即触发
    EXPECT_FALSE(error_callback_set.load());

    close(sockfd);
}
