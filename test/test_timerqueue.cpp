#include <gtest/gtest.h>
#include "net/EventLoop.hpp"
#include "net/Timer.hpp"
#include "net/TimerQueue.hpp"
#include <atomic>
#include <chrono>
#include <thread>
#include <vector>
#include <algorithm>

using namespace hyperMuduo::net;
using namespace std::chrono_literals;

// Test 1: One-shot timer fires correctly
TEST(TimerQueueTest, OneShotTimer) {
    EventLoop loop;
    TimerQueue queue(loop);
    std::atomic<bool> fired{false};

    queue.addTimer(
        [&]() {
            fired = true;
            loop.quit();
        },
        std::chrono::steady_clock::now() + 50ms,
        0ms
    );

    auto begin = std::chrono::steady_clock::now();
    loop.loop();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - begin).count();

    EXPECT_TRUE(fired.load());
    EXPECT_GE(elapsed, 40);  // Allow small timing variance
    EXPECT_LT(elapsed, 200);
}

// Test 2: Repeated timer fires multiple times
TEST(TimerQueueTest, RepeatedTimer) {
    EventLoop loop;
    TimerQueue queue(loop);
    std::atomic<int> count{0};

    queue.addTimer(
        [&]() {
            int c = ++count;
            if (c >= 3) {
                loop.quit();
            }
        },
        std::chrono::steady_clock::now() + 30ms,
        30ms
    );

    loop.loop();
    EXPECT_GE(count.load(), 3);
}

// Test 3: Cancel timer before it fires
TEST(TimerQueueTest, CancelBeforeFire) {
    EventLoop loop;
    TimerQueue queue(loop);
    std::atomic<bool> fired{false};

    auto timer_id = queue.addTimer(
        [&]() {
            fired = true;
        },
        std::chrono::steady_clock::now() + 500ms,
        0ms
    );

    // Cancel immediately
    queue.cancelTimer(timer_id);

    // Add a quit timer
    queue.addTimer(
        [&]() {
            loop.quit();
        },
        std::chrono::steady_clock::now() + 100ms,
        0ms
    );
    
    loop.loop();

    EXPECT_FALSE(fired.load());
}

// Test 4: Multiple timers with different deadlines
TEST(TimerQueueTest, MultipleTimers) {
    EventLoop loop;
    TimerQueue queue(loop);
    std::vector<int> order;
    std::mutex mtx;
    std::atomic<int> fired_count{0};

    queue.addTimer(
        [&]() {
            std::lock_guard<std::mutex> lock(mtx);
            order.push_back(2);
            ++fired_count;
        },
        std::chrono::steady_clock::now() + 100ms,
        0ms
    );

    queue.addTimer(
        [&]() {
            std::lock_guard<std::mutex> lock(mtx);
            order.push_back(1);
            ++fired_count;
        },
        std::chrono::steady_clock::now() + 50ms,
        0ms
    );

    // Add a quit timer after both
    queue.addTimer(
        [&]() {
            loop.quit();
        },
        std::chrono::steady_clock::now() + 150ms,
        0ms
    );

    loop.loop();

    // Both timers should have fired
    std::lock_guard<std::mutex> lock(mtx);
    EXPECT_EQ(fired_count.load(), 2);
    // Timer with 50ms should fire before 100ms
    if (order.size() == 2) {
        EXPECT_EQ(order[0], 1);
        EXPECT_EQ(order[1], 2);
    }
}

// Test 5: Cancel repeated timer after some fires
TEST(TimerQueueTest, CancelRepeatedTimer) {
    EventLoop loop;
    TimerQueue queue(loop);
    std::atomic<int> count{0};

    // 1. 添加一个每 20ms 执行一次的循环定时器
    auto timer_id = queue.addTimer(
        [&count]() { ++count; },
        std::chrono::steady_clock::now() + 20ms,
        20ms
    );

    // 2. 添加一个 70ms 后执行的单次定时器，专门用来“暗杀”上面那个循环定时器
    // 此时 count 应该加了 3 次 (20ms, 40ms, 60ms)
    queue.addTimer(
        [&queue, timer_id]() { queue.cancelTimer(timer_id); },
        std::chrono::steady_clock::now() + 70ms,
        0ms
    );

    // 3. 添加一个 150ms 后退出的定时器，确保循环定时器真的停了
    queue.addTimer(
        [&loop]() { loop.quit(); },
        std::chrono::steady_clock::now() + 150ms,
        0ms
    );

    loop.loop();

    // 预期：被取消后，count 应该停在 3，绝对达不到 7（150/20）
    EXPECT_EQ(count.load(), 3);
}

// Test 6: Timer callback runs in EventLoop thread
TEST(TimerQueueTest, TimerCallbackInLoopThread) {
    EventLoop loop;
    TimerQueue queue(loop);
    std::atomic<std::thread::id> callback_thread_id;
    std::thread::id loop_thread_id;

    queue.addTimer(
        [&]() {
            callback_thread_id = std::this_thread::get_id();
            loop.quit();
        },
        std::chrono::steady_clock::now() + 50ms,
        0ms
    );

    loop.loop();

    EXPECT_EQ(callback_thread_id.load(), std::this_thread::get_id());
}

// Test 7: Very short duration timer
TEST(TimerQueueTest, VeryShortTimer) {
    EventLoop loop;
    TimerQueue queue(loop);
    std::atomic<bool> fired{false};

    queue.addTimer(
        [&]() {
            fired = true;
            loop.quit();
        },
        std::chrono::steady_clock::now() + 1ms,
        0ms
    );

    loop.loop();
    EXPECT_TRUE(fired.load());
}

// Test 8: Cancel non-existent timer (should not crash)
TEST(TimerQueueTest, CancelNonExistentTimer) {
    EventLoop loop;
    TimerQueue queue(loop);
    
    // Create and cancel a timer
    auto timer_id = queue.addTimer(
        []() {},
        std::chrono::steady_clock::now() + 1000ms,
        0ms
    );
    
    queue.cancelTimer(timer_id);
    // Cancel again should be safe
    queue.cancelTimer(timer_id);
}

// Test 9: Timer with capture in callback
TEST(TimerQueueTest, TimerCallbackWithCapture) {
    EventLoop loop;
    TimerQueue queue(loop);
    std::shared_ptr<int> shared_value = std::make_shared<int>(42);
    std::atomic<int> result{0};

    queue.addTimer(
        [shared_value, &result, &loop]() {
            result = *shared_value * 2;
            loop.quit();
        },
        std::chrono::steady_clock::now() + 50ms,
        0ms
    );

    loop.loop();
    EXPECT_EQ(result.load(), 84);
}

// Test 10: Stress test - many timers added at once
TEST(TimerQueueTest, StressManyTimers) {
    EventLoop loop;
    TimerQueue queue(loop);
    std::atomic<int> count{0};
    constexpr int num_timers = 100;

    for (int i = 0; i < num_timers; ++i) {
        queue.addTimer(
            [&count]() {
                ++count;
            },
            std::chrono::steady_clock::now() + 50ms,
            0ms
        );
    }

    // Add a final timer to quit the loop
    queue.addTimer(
        [&]() {
            loop.quit();
        },
        std::chrono::steady_clock::now() + 100ms,
        0ms
    );

    loop.loop();
    EXPECT_EQ(count.load(), num_timers);
}
// Test 11: Cross-thread timer addition and cancellation
TEST(TimerQueueTest, CrossThreadSafety) {
    EventLoop loop;
    TimerQueue queue(loop);
    std::atomic<int> fired_count{0};

    // 1. 让 EventLoop 设定在 200ms 后退出
    queue.addTimer([&loop]() { loop.quit(); }, std::chrono::steady_clock::now() + 200ms, 0ms);

    // 2. 开启一个后台打工人线程
    std::thread worker([&]() {
        // 故意睡 50ms，等 EventLoop 已经在 loop() 里死睡阻塞了再发难
        std::this_thread::sleep_for(50ms);

        // 跨线程添加一个 50ms 后触发的定时器（利用 runInLoop 唤醒）
        auto target_id = queue.addTimer(
            [&fired_count]() { ++fired_count; },
            std::chrono::steady_clock::now() + 50ms,
            0ms
        );

        // 再睡 20ms，然后跨线程无情取消它！（此时它还没来得及触发）
        std::this_thread::sleep_for(20ms);
        queue.cancelTimer(target_id);
    });

    // 3. 启动 IO 线程
    loop.loop();
    worker.join(); // 等待后台线程结束

    // 预期：定时器在后台线程被成功取消，回调绝对没有被执行过！
    EXPECT_EQ(fired_count.load(), 0);
}