#include <gtest/gtest.h>
#include "net/EventLoop.hpp"
#include "net/Channel.hpp"
#include <atomic>
#include <chrono>
#include <thread>
#include <unistd.h>

TEST(EventLoopTest, SingleChannelDispatch) {
    int fds[2];
    ASSERT_EQ(::pipe(fds), 0);
    hyperMuduo::net::EventLoop loop;
    hyperMuduo::net::Channel channel(loop, fds[0]);
    std::atomic<bool> handled{false};
    channel.setReadCallback([&](std::chrono::system_clock::time_point) {
        char c = 0;
        ::read(fds[0], &c, 1);
        handled = true;
        loop.quit();
    });
    channel.listenTillReadable();

    std::thread writer([&]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
        const char c = 'x';
        ::write(fds[1], &c, 1);
    });

    loop.loop();
    writer.join();
    ::close(fds[0]);
    ::close(fds[1]);
    EXPECT_TRUE(handled.load());
}

TEST(EventLoopTest, MultiChannelDispatch) {
    int p1[2];
    int p2[2];
    ASSERT_EQ(::pipe(p1), 0);
    ASSERT_EQ(::pipe(p2), 0);
    
    hyperMuduo::net::EventLoop loop;
    hyperMuduo::net::Channel ch1(loop, p1[0]);
    hyperMuduo::net::Channel ch2(loop, p2[0]);
    std::atomic<int> handled_count{0};

    ch1.setReadCallback([&](std::chrono::system_clock::time_point) {
        char c = 0;
        ::read(p1[0], &c, 1);
        ++handled_count;
        if (handled_count == 2) {
            loop.quit();
        }
    });
    ch2.setReadCallback([&](std::chrono::system_clock::time_point) {
        char c = 0;
        ::read(p2[0], &c, 1);
        ++handled_count;
        if (handled_count == 2) {
            loop.quit();
        }
    });
    ch1.listenTillReadable();
    ch2.listenTillReadable();

    std::thread writer([&]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(15));
        const char a = 'a';
        const char b = 'b';
        ::write(p1[1], &a, 1);
        ::write(p2[1], &b, 1);
    });

    loop.loop();
    writer.join();
    ::close(p1[0]);
    ::close(p1[1]);
    ::close(p2[0]);
    ::close(p2[1]);
    EXPECT_EQ(handled_count.load(), 2);
}

TEST(EventLoopTest, QuitFromOtherThread) {
    hyperMuduo::net::EventLoop loop;
    auto begin = std::chrono::steady_clock::now();

    std::thread stopper([&]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(30));
        loop.quit();
    });

    loop.loop();
    stopper.join();
    const auto elapsed_ms =
        std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - begin).count();
    EXPECT_GE(elapsed_ms, 20);
    EXPECT_LT(elapsed_ms, 500);
}

TEST(EventLoopTest, ThreadAffinityFlags) {
    hyperMuduo::net::EventLoop loop;
    const bool in_main_thread = loop.isInLoopThread();
    std::atomic<bool> in_worker_thread{true};

    std::thread worker([&]() {
        in_worker_thread = loop.isInLoopThread();
    });
    worker.join();
    EXPECT_TRUE(in_main_thread);
    EXPECT_FALSE(in_worker_thread.load());
}
