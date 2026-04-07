#include <gtest/gtest.h>
#include "net/Buffer.hpp"
#include "net/protobuf/Codec.hpp"
#include "test.pb.h"

TEST(CodecTest, RoundTrip) {
    hyperMuduo::net::Buffer wire;
    hyper::Query query;
    query.set_id(42);
    query.set_questioner("zhr");
    query.set_content("hello muduo");

    hyperMuduo::net::Codec::Encode(query, wire);
    auto [state, decoded] = hyperMuduo::net::Codec::Decode(wire);
    EXPECT_EQ(state, hyperMuduo::net::Codec::DecodeState::SUCCESS);
    ASSERT_TRUE(decoded);
    auto out = std::dynamic_pointer_cast<hyper::Query>(decoded);
    EXPECT_TRUE(out);
    EXPECT_EQ(out->id(), 42);
    EXPECT_EQ(out->questioner(), "zhr");
    EXPECT_EQ(out->content(), "hello muduo");
}
