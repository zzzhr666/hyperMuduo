#include <gtest/gtest.h>
#include "net/EventLoopThread.hpp"
#include "net/EventLoop.hpp"
#include <atomic>
#include <chrono>
#include <thread>

TEST(EventLoopThreadTest, BasicStartAndQuit) {
    hyperMuduo::net::EventLoopThread thread("TestThread-Basic", [](hyperMuduo::net::EventLoop& loop) {
        // Init callback - can set up timers or other components
    });

    hyperMuduo::net::EventLoop& loop = thread.startLoop();
    
    // Give the loop some time to run
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    
    // loop.quit() will be called by destructor
}

TEST(EventLoopThreadTest, InitCallback) {
    std::atomic<bool> init_called{false};
    
    hyperMuduo::net::EventLoopThread thread(
        "TestThread-InitCallback",
        [&](hyperMuduo::net::EventLoop& loop) {
            init_called = true;
        }
    );
    
    hyperMuduo::net::EventLoop& loop = thread.startLoop();
    
    // Wait a bit for init callback to execute
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    
    // Verify init callback was called
    EXPECT_TRUE(init_called.load());
}

TEST(EventLoopThreadTest, RunTaskOnLoopThread) {
    std::atomic<bool> task_executed{false};
    std::atomic<bool> task_in_correct_thread{false};
    std::thread::id loop_thread_id;
    
    hyperMuduo::net::EventLoopThread thread(
        "TestThread-RunTask",
        [&](hyperMuduo::net::EventLoop& loop) {
            loop_thread_id = std::this_thread::get_id();

            // Schedule a task from within the loop thread
            loop.runInLoop([&]() {
                task_executed = true;
                task_in_correct_thread = (std::this_thread::get_id() == loop_thread_id);
            });
        }
    );
    
    hyperMuduo::net::EventLoop& loop = thread.startLoop();
    
    // Wait for task to execute
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    
    EXPECT_TRUE(task_executed.load());
    EXPECT_TRUE(task_in_correct_thread.load());
}

TEST(EventLoopThreadTest, RunTaskFromMainThread) { // 改个名字
    std::atomic<bool> task_executed{false};
    std::atomic<bool> task_in_correct_thread{false};
    std::thread::id loop_thread_id;

    hyperMuduo::net::EventLoopThread thread(
        "TestThread-RunTaskFromMain",
        [&](hyperMuduo::net::EventLoop& loop) {
            // 仅仅记录后台线程的 ID
            loop_thread_id = std::this_thread::get_id();
        }
    );

    hyperMuduo::net::EventLoop& loop = thread.startLoop();

    // 核心大考：主线程跨线程向 EventLoop 投递任务！
    loop.runInLoop([&]() {
        task_executed = true;
        // 断言这个任务确确实实是在后台线程执行的
        task_in_correct_thread = (std::this_thread::get_id() == loop_thread_id);
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    EXPECT_TRUE(task_executed.load());
    EXPECT_TRUE(task_in_correct_thread.load());
}
TEST(EventLoopThreadTest, MultipleThreads) {
    std::atomic<int> loop_count{0};
    
    hyperMuduo::net::EventLoopThread thread1(
        "TestThread-1",
        [&](hyperMuduo::net::EventLoop& loop) {
            loop.runAfter(std::chrono::milliseconds(50), [&]() {
                ++loop_count;
            });
        }
    );

    hyperMuduo::net::EventLoopThread thread2(
        "TestThread-2",
        [&](hyperMuduo::net::EventLoop& loop) {
            loop.runAfter(std::chrono::milliseconds(50), [&]() {
                ++loop_count;
            });
        }
    );
    
    hyperMuduo::net::EventLoop& loop1 = thread1.startLoop();
    hyperMuduo::net::EventLoop& loop2 = thread2.startLoop();
    
    // Wait for both timers to fire
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    EXPECT_EQ(loop_count.load(), 2);
}

TEST(EventLoopThreadTest, DestructorStopsThread) {
    std::atomic<int> timer_count{0};
    
    {
        hyperMuduo::net::EventLoopThread thread(
            "TestThread-Destructor",
            [&](hyperMuduo::net::EventLoop& loop) {
                loop.runEvery(std::chrono::milliseconds(30), [&]() {
                    ++timer_count;
                });
            }
        );
        
        hyperMuduo::net::EventLoop& loop = thread.startLoop();
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        
        // Destructor will call loop.quit() and thread.join()
    }
    
    int count_after_destruct = timer_count.load();
    
    // Wait a bit more to ensure no more timers fire
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    // Count should not increase after destructor
    EXPECT_EQ(timer_count.load(), count_after_destruct);
}
