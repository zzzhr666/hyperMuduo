#include <gtest/gtest.h>
#include "net/protobuf/Codec.hpp"
#include "net/protobuf/ProtobufHandler.hpp"
#include "net/Buffer.hpp"
#include "test.pb.h"

TEST(ProtobufTest, HandlerDispatch) {
    int query_called = 0;
    int default_called = 0;
    
    hyperMuduo::net::ProtobufHandler handler(
        [&](const hyperMuduo::net::TcpConnectionPtr&, const hyperMuduo::net::MessagePtr&, hyperMuduo::net::TimePoint) {
            ++default_called;
        });
        
    handler.SetCallback<hyper::Query>(
        [&](const hyperMuduo::net::TcpConnectionPtr&, const std::shared_ptr<hyper::Query>& msg, hyperMuduo::net::TimePoint) {
            if (msg && msg->id() == 7) {
                ++query_called;
            }
        });

    hyperMuduo::net::Buffer in;
    hyper::Query query;
    query.set_id(7);
    query.set_questioner("reader");
    query.set_content("dispatch");
    hyperMuduo::net::Codec::Encode(query, in);

    handler.OnMessage({}, in, std::chrono::system_clock::now());
    
    EXPECT_EQ(query_called, 1);
    EXPECT_EQ(default_called, 0);
}
