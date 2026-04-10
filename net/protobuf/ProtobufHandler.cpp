#include "ProtobufHandler.hpp"

hyperMuduo::net::ProtobufHandler::ProtobufHandler(
    std::function<void(const TcpConnectionPtr&, const MessagePtr&, TimePoint)> default_cb)
    : dispatcher_{std::make_unique<Dispatcher>(default_cb)} {
}

void hyperMuduo::net::ProtobufHandler::OnMessage(const TcpConnectionPtr& connection,
                                            Buffer& buffer,
                                            TimePoint timestamp) const {
    while (buffer.readableBytes() >= Codec::HEADER_LEN) {
        auto [state , msg] = Codec::Decode(buffer);
        if (state == Codec::DecodeState::INCOMPLETE) {
            return;
        }
        if (state == Codec::DecodeState::SUCCESS) {
            dispatcher_->Dispatch(connection,msg,timestamp);
        } else {
            std::cerr << "ProtobufHandler::OnMessage Decode error:"<<static_cast<int>(state) << std::endl;
            return;
        }

    }
}

void hyperMuduo::net::ProtobufHandler::Send(const TcpConnectionPtr& connection, const google::protobuf::Message& msg) {
    Codec::Encode(msg, connection->sendBuffer());
}
