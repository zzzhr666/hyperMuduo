#include <gtest/gtest.h>
#include "net/Buffer.hpp"

TEST(BufferTest, BasicReadWrite) {
    hyperMuduo::net::Buffer buffer;
    buffer.append("abc", 3);
    EXPECT_EQ(buffer.readableBytes(), 3);
    auto msg = buffer.retrieveAllAsString();
    EXPECT_EQ(msg, "abc");
    EXPECT_EQ(buffer.readableBytes(), 0);
}

TEST(BufferTest, PeekAndRetrieve) {
    hyperMuduo::net::Buffer buffer;
    buffer.append("hello world", 11);
    EXPECT_EQ(std::string(buffer.peek(), 5), "hello");
    buffer.retrieve(6);
    EXPECT_EQ(buffer.retrieveAllAsString(), "world");
}
