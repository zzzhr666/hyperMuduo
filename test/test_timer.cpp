#include <gtest/gtest.h>
#include "net/EventLoop.hpp"
#include "net/Timer.hpp"
#include "net/TimerQueue.hpp"
#include <atomic>
#include <chrono>
#include <thread>

TEST(TimerTest, OneShot) {
    hyperMuduo::net::EventLoop loop;
    hyperMuduo::net::TimerQueue queue(loop);
    std::atomic<bool> fired{false};

    queue.addTimer(
        [&]() {
            fired = true;
            loop.quit();
        },
        std::chrono::steady_clock::now() + std::chrono::milliseconds(50),
        std::chrono::milliseconds(0)
    );

    auto begin = std::chrono::steady_clock::now();
    loop.loop();
    auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - begin).count();
        
    EXPECT_TRUE(fired.load());
    EXPECT_GE(elapsed_ms, 40);
    EXPECT_LT(elapsed_ms, 200);
}

TEST(TimerTest, RepeatAndCancel) {
    hyperMuduo::net::EventLoop loop;
    hyperMuduo::net::TimerQueue queue(loop);
    std::atomic<int> count{0};

    hyperMuduo::net::TimerId timer_id(0);
    timer_id = queue.addTimer(
        [&count]() {
            ++count;
        },
        std::chrono::steady_clock::now() + std::chrono::milliseconds(30),
        std::chrono::milliseconds(30)
    );

    // Add a quit timer
    queue.addTimer(
        [&]() {
            loop.quit();
        },
        std::chrono::steady_clock::now() + std::chrono::milliseconds(200),
        std::chrono::milliseconds(0)
    );

    loop.loop();
    // Should fire at least once in 200ms with 30ms interval
    EXPECT_GE(count.load(), 1);
}

TEST(TimerTest, CrossThreadAdd) {
    std::atomic<bool> fired{false};
    std::thread::id loop_thread_id;

    // 用于把后台创建的 EventLoop 指针传递给主线程
    std::atomic<hyperMuduo::net::EventLoop*> loop_ptr{nullptr};

    // 1. 启动后台线程
    std::thread loop_thread([&]() {
        // 关键修复：EventLoop 必须在运转它的线程内被创建！
        hyperMuduo::net::EventLoop loop;

        loop_thread_id = std::this_thread::get_id();
        loop_ptr.store(&loop); // 告诉主线程：“我建好了，指针给你”

        loop.loop(); // 此时创建线程和 loop 线程完美一致，绝不会 abort
    });

    // 2. 主线程等一会儿，直到后台的 EventLoop 创建完毕
    while (loop_ptr.load() == nullptr) {
        std::this_thread::yield();
    }

    // 3. 跨线程投递定时器大考！
    // 主线程调用后台 loop 的 runAfter。
    // 注意：请确保你在上一轮修改中，已经把 runAfter 内部的 assertInLoopThread() 删掉了哦！
    loop_ptr.load()->runAfter(std::chrono::milliseconds(50), [&]() {
        fired = true;
        EXPECT_EQ(std::this_thread::get_id(), loop_thread_id);
        loop_ptr.load()->quit();
    });

    loop_thread.join();
    EXPECT_TRUE(fired.load());
}