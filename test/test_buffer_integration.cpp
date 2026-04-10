#include <gtest/gtest.h>
#include <unistd.h>
#include <sys/socket.h>
#include <thread>
#include <chrono>
#include <string>
#include "net/Buffer.hpp"

using namespace hyperMuduo::net;

// Test 1: 管道读写集成
TEST(BufferIntegrationTest, ReadWriteThroughPipe) {
    int fds[2];
    ASSERT_EQ(pipe(fds), 0);

    Buffer buf;
    std::string test_data = "Hello from pipe!";

    // 写入管道
    ssize_t n = write(fds[1], test_data.data(), test_data.size());
    EXPECT_EQ(n, test_data.size());

    // 从管道读取到 Buffer
    int saved_errno = 0;
    ssize_t bytes_read = buf.readFd(fds[0], &saved_errno);

    EXPECT_GT(bytes_read, 0);
    EXPECT_EQ(buf.readableBytes(), test_data.size());
    EXPECT_EQ(buf.retrieveAsString(bytes_read), test_data);

    close(fds[0]);
    close(fds[1]);
}

// Test 2: 大数据块传输（超过初始buffer大小）
TEST(BufferIntegrationTest, LargeDataTransfer) {
    int fds[2];
    ASSERT_EQ(pipe(fds), 0);

    Buffer buf;
    // 创建一个大于初始缓冲区大小的数据
    std::string large_data(2048, 'x');

    // 写入管道
    ssize_t n = write(fds[1], large_data.data(), large_data.size());
    EXPECT_EQ(n, large_data.size());

    // 读取到 Buffer
    int saved_errno = 0;
    ssize_t bytes_read = buf.readFd(fds[0], &saved_errno);

    EXPECT_GT(bytes_read, 0);
    EXPECT_EQ(buf.readableBytes(), large_data.size());

    std::string result = buf.retrieveAllAsString();
    EXPECT_EQ(result, large_data);

    close(fds[0]);
    close(fds[1]);
}

// Test 3: prepend 功能（模拟添加协议头）
TEST(BufferIntegrationTest, PrependForProtocolHeader) {
    Buffer buf;

    // 先添加一些数据
    std::string payload = "Payload data";
    buf.append(payload.data(), payload.size());

    EXPECT_EQ(buf.readableBytes(), payload.size());
    EXPECT_EQ(buf.prependableBytes(), Buffer::CHEAP_PREPEND);

    // 在数据前面添加协议头
    int32_t header = 12345;
    buf.prepend(&header, sizeof(header));

    // 验证prepend后的数据布局
    EXPECT_EQ(buf.readableBytes(), payload.size() + sizeof(header));

    // 读取header
    int32_t read_header;
    memcpy(&read_header, buf.peek(), sizeof(header));
    buf.retrieve(sizeof(header));

    EXPECT_EQ(read_header, 12345);
    EXPECT_EQ(buf.retrieveAllAsString(), payload);
}

// Test 4: swap 零拷贝转移
TEST(BufferIntegrationTest, SwapZeroCopy) {
    Buffer buf1;
    Buffer buf2;

    // buf1 有数据
    std::string data1 = "Data from buf1";
    buf1.append(data1.data(), data1.size());

    // buf2 也有数据
    std::string data2 = "Data from buf2";
    buf2.append(data2.data(), data2.size());

    // 交换
    buf1.swap(buf2);

    // 验证交换后的数据
    EXPECT_EQ(buf1.readableBytes(), data2.size());
    EXPECT_EQ(buf1.retrieveAllAsString(), data2);

    EXPECT_EQ(buf2.readableBytes(), data1.size());
    EXPECT_EQ(buf2.retrieveAllAsString(), data1);
}

// Test 5: Buffer 状态查询
TEST(BufferIntegrationTest, BufferStateQueries) {
    Buffer buf;

    // 初始状态
    EXPECT_EQ(buf.prependableBytes(), Buffer::CHEAP_PREPEND);
    EXPECT_GT(buf.writableBytes(), 0);
    EXPECT_EQ(buf.readableBytes(), 0);

    // 添加数据后
    std::string data = "Test data";
    buf.append(data.data(), data.size());

    EXPECT_EQ(buf.readableBytes(), data.size());
    EXPECT_LT(buf.writableBytes(), Buffer::INITIALIZE_SIZE);

    // 读取部分数据
    buf.retrieve(4);
    EXPECT_EQ(buf.readableBytes(), data.size() - 4);

    // 清空
    buf.retrieveAll();
    EXPECT_EQ(buf.readableBytes(), 0);
    EXPECT_EQ(buf.prependableBytes(), Buffer::CHEAP_PREPEND);
}

// Test 6: ensureWritableBytes 自动扩容
TEST(BufferIntegrationTest, EnsureWritableBytesAutoExpand) {
    Buffer buf;

    size_t initial_writable = buf.writableBytes();

    // 请求一个大于当前可写空间的字节数
    buf.ensureWritableBytes(initial_writable + 100);

    // Buffer 应该已经扩容
    EXPECT_GE(buf.writableBytes(), initial_writable + 100);
}

// Test 7: hasWritten 手动更新写指针
TEST(BufferIntegrationTest, HasWrittenManualUpdate) {
    Buffer buf;

    // 模拟直接写入buffer底层数据
    char* write_ptr = buf.beginWrite();
    std::string data = "Manual write";
    memcpy(write_ptr, data.data(), data.size());

    // 手动更新写指针
    buf.hasWritten(data.size());

    // 验证数据可读
    EXPECT_EQ(buf.readableBytes(), data.size());
    EXPECT_EQ(buf.retrieveAsString(data.size()), data);
}
