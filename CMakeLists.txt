cmake_minimum_required(VERSION 3.12)
project(HyperMuduo)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_INTERPROCEDURAL_OPTIMIZATION TRUE)

enable_testing()
find_package(Threads REQUIRED)
find_package(Protobuf REQUIRED)
find_package(PkgConfig REQUIRED)
find_package(ZLIB REQUIRED)
find_package(spdlog REQUIRED)
pkg_check_modules(PROTOBUF REQUIRED IMPORTED_TARGET protobuf)

protobuf_generate_cpp(PROTO_SRCS PROTO_HDRS proto/test.proto)

set(BASE_SOURCE
        Base/CurrentThread.cpp
)

set(NET_SOURCE
        net/Buffer.cpp
        net/TcpConnection.cpp
        net/EventLoop.cpp
        net/Channel.cpp
        net/Poller.cpp
        net/Timer.cpp
        net/TimerQueue.cpp
        net/EventLoopThread.cpp
        net/Acceptor.cpp
        net/Socket.cpp
        net/InetAddress.cpp
        net/TcpServer.cpp
        net/EventLoopThreadPool.cpp
        net/Connector.cpp
        net/TcpClient.cpp
        net/Epoller.cpp
        net/Epoller.hpp

)

set(PROTO_SOURCE
        ${PROTO_SRCS}
        net/protobuf/Codec.cpp
        net/protobuf/Dispatcher.cpp
        net/protobuf/ProtobufHandler.cpp

)


add_library(hyper_muduo STATIC
        ${NET_SOURCE}
        ${PROTO_SOURCE}
        ${BASE_SOURCE}
)
target_include_directories(hyper_muduo
        PUBLIC
        ${CMAKE_CURRENT_BINARY_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(hyper_muduo
        PUBLIC
        Threads::Threads
        PkgConfig::PROTOBUF
        ZLIB::ZLIB
        spdlog::spdlog
)


add_subdirectory(test)
add_subdirectory(benchmark)
