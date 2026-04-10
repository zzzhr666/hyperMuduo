#include <gtest/gtest.h>
#include <thread>
#include <chrono>
#include <memory>
#include "net/protobuf/ProtobufHandler.hpp"
#include "net/Buffer.hpp"
#include "net/TcpConnection.hpp"
#include "net/EventLoop.hpp"
#include "net/Socket.hpp"
#include "net/InetAddress.hpp"
#include "test.pb.h"

using namespace hyperMuduo::net;
using namespace hyper;

// 辅助：创建模拟的 TcpConnection
std::shared_ptr<TcpConnection> create_mock_connection(EventLoop& loop) {
    int fds[2];
    pipe(fds);

    Socket server_socket(fds[0]);
    InetAddress local_addr("127.0.0.1", 20010);
    InetAddress peer_addr("127.0.0.1", 0);

    auto conn = std::make_shared<TcpConnection>(loop, "mock#1", std::move(server_socket), local_addr, peer_addr);
    conn->connectionEstablished();

    // 关闭读端，因为我们只用来测试编解码
    close(fds[1]);

    return conn;
}

// Test 1: 注册并分发 Query 消息
TEST(ProtobufHandlerTest, EncodeDecodeQueryMessage) {
    EventLoop loop;
    auto conn = create_mock_connection(loop);

    std::atomic<bool> callback_triggered{false};
    Query received_query;

    // 创建 Handler 并注册 Query 回调
    ProtobufHandler handler(nullptr);
    handler.SetCallback<Query>([&](const TcpConnectionPtr&, const std::shared_ptr<Query>& query, TimePoint) {
        received_query.CopyFrom(*query);
        callback_triggered = true;
    });

    // 创建 Query 消息
    Query test_query;
    test_query.set_id(12345);
    test_query.set_questioner("Alice");
    test_query.set_content("What is the meaning of life?");

    // 编码消息到 Buffer
    Buffer buffer;
    Codec::Encode(test_query, buffer);

    // 验证编码后的数据不为空
    EXPECT_GT(buffer.readableBytes(), 0);

    // 通过 Handler 解码和分发
    handler.OnMessage(conn, buffer, std::chrono::system_clock::now());

    EXPECT_TRUE(callback_triggered.load());
    EXPECT_EQ(received_query.id(), 12345);
    EXPECT_EQ(received_query.questioner(), "Alice");
    EXPECT_EQ(received_query.content(), "What is the meaning of life?");
}

// Test 2: 注册多种消息类型并验证分发
TEST(ProtobufHandlerTest, MultiMessageTypeDispatch) {
    EventLoop loop;
    auto conn = create_mock_connection(loop);

    std::atomic<int> query_count{0};
    std::atomic<int> answer_count{0};

    Query received_query;
    Answer received_answer;

    // 创建 Handler
    ProtobufHandler handler(nullptr);

    // 注册 Query 回调
    handler.SetCallback<Query>([&](const TcpConnectionPtr&, const std::shared_ptr<Query>& query, TimePoint) {
        received_query.CopyFrom(*query);
        query_count++;
    });

    // 注册 Answer 回调
    handler.SetCallback<Answer>([&](const TcpConnectionPtr&, const std::shared_ptr<Answer>& answer, TimePoint) {
        received_answer.CopyFrom(*answer);
        answer_count++;
    });

    // 编码 Query 消息
    Query test_query;
    test_query.set_id(1);
    test_query.set_questioner("Bob");
    test_query.set_content("Query message");

    Buffer query_buffer;
    Codec::Encode(test_query, query_buffer);

    // 编码 Answer 消息
    Answer test_answer;
    test_answer.set_id(2);
    test_answer.set_responder("Charlie");
    test_answer.set_content("Answer message");

    Buffer answer_buffer;
    Codec::Encode(test_answer, answer_buffer);

    // 分发 Query
    handler.OnMessage(conn, query_buffer, std::chrono::system_clock::now());
    EXPECT_EQ(query_count.load(), 1);
    EXPECT_EQ(answer_count.load(), 0);

    // 分发 Answer
    handler.OnMessage(conn, answer_buffer, std::chrono::system_clock::now());
    EXPECT_EQ(query_count.load(), 1);
    EXPECT_EQ(answer_count.load(), 1);

    // 验证数据正确性
    EXPECT_EQ(received_query.id(), 1);
    EXPECT_EQ(received_answer.id(), 2);
}

// Test 3: 默认回调处理未注册的消息
TEST(ProtobufHandlerTest, DefaultCallbackForUnregisteredMessage) {
    EventLoop loop;
    auto conn = create_mock_connection(loop);

    std::atomic<bool> default_callback_triggered{false};

    // 创建 Handler，设置默认回调
    ProtobufHandler handler([&](const TcpConnectionPtr&, const MessagePtr&, TimePoint) {
        default_callback_triggered = true;
    });

    // 编码一个 Query 消息（但没有注册对应的回调）
    Query test_query;
    test_query.set_id(999);
    test_query.set_questioner("Unknown");
    test_query.set_content("No handler registered");

    Buffer buffer;
    Codec::Encode(test_query, buffer);

    // 分发
    handler.OnMessage(conn, buffer, std::chrono::system_clock::now());

    // 默认回调应该被触发
    EXPECT_TRUE(default_callback_triggered.load());
}

// Test 4: 连续处理多条消息
TEST(ProtobufHandlerTest, ConsecutiveMessageProcessing) {
    EventLoop loop;
    auto conn = create_mock_connection(loop);

    std::vector<std::string> received_messages;

    // 创建 Handler
    ProtobufHandler handler(nullptr);
    handler.SetCallback<Query>([&](const TcpConnectionPtr&, const std::shared_ptr<Query>& query, TimePoint) {
        received_messages.push_back(query->content());
    });

    // 创建多条消息并编码到一个 Buffer
    Buffer combined_buffer;

    for (int i = 1; i <= 5; i++) {
        Query query;
        query.set_id(i);
        query.set_questioner("User" + std::to_string(i));
        query.set_content("Message #" + std::to_string(i));

        Codec::Encode(query, combined_buffer);
    }

    // 验证 Buffer 中有足够数据
    EXPECT_GT(combined_buffer.readableBytes(), 0);

    // 一次性处理所有消息
    handler.OnMessage(conn, combined_buffer, std::chrono::system_clock::now());

    // 应该收到所有5条消息
    EXPECT_EQ(received_messages.size(), 5);
    EXPECT_EQ(received_messages[0], "Message #1");
    EXPECT_EQ(received_messages[4], "Message #5");

    // Buffer 应该已经被清空
    EXPECT_EQ(combined_buffer.readableBytes(), 0);
}

// Test 5: 发送消息（验证编码流程）
TEST(ProtobufHandlerTest, SendMessageThroughConnection) {
    EventLoop loop;
    auto conn = create_mock_connection(loop);

    // 创建 Handler
    ProtobufHandler handler(nullptr);

    // 创建并发送消息
    Query test_query;
    test_query.set_id(100);
    test_query.set_questioner("Sender");
    test_query.set_content("Test send functionality");

    // 使用 Handler 发送消息到连接
    handler.Send(conn, test_query);

    // 验证 send buffer 中有数据（send_buffer_ 在构造时即已初始化）
    Buffer& send_buf = conn->sendBuffer();
    EXPECT_GT(send_buf.readableBytes(), 0);

    // 验证数据格式正确（至少应该有最小长度）
    EXPECT_GE(send_buf.readableBytes(), Codec::MIN_LEN);
}
