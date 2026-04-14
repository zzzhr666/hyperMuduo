find_package(GTest REQUIRED)

# 通用测试函数
function(add_hyper_test test_name test_files)
    add_executable(${test_name} ${test_files})
    target_link_libraries(${test_name}
        PRIVATE
        hyper_muduo
        GTest::gtest
        GTest::gtest_main
    )
    add_test(NAME ${test_name} COMMAND ${test_name})
endfunction()

# ==================== 各组件独立测试 ====================

# Buffer 测试
add_hyper_test(BufferTests test_buffer.cpp)

# Codec 测试
add_hyper_test(CodecTests test_codec.cpp)

# Protobuf 测试
add_hyper_test(ProtobufTests test_protobuf.cpp)

# EventLoop 测试
add_hyper_test(EventLoopTests test_event_loop.cpp)

# Timer 测试
add_hyper_test(TimerTests test_timer.cpp)

# TimerQueue 测试
add_hyper_test(TimerQueueTests test_timerqueue.cpp)

# EventLoopThread 测试
add_hyper_test(EventLoopThreadTests test_eventloopthread.cpp)

# InetAddress 测试
add_hyper_test(InetAddressTests test_inetaddress.cpp)

# Socket 测试
add_hyper_test(SocketTests test_socket.cpp)

# Acceptor 测试
add_hyper_test(AcceptorTests test_acceptor.cpp)

# TcpServer 集成测试
add_hyper_test(TcpServerTests test_tcpserver.cpp)

# TcpServer 多线程测试
add_hyper_test(MultiThreadTcpServerTests test_tcpserver_multithread.cpp)

# TcpServer 高级应用测试
add_hyper_test(TcpServerAdvancedTests test_tcpserver_advanced.cpp)

# TcpConnection 生命周期测试
add_hyper_test(TcpConnectionTests test_tcpconnection.cpp)

# Buffer 网络IO测试
add_hyper_test(BufferIntegrationTests test_buffer_integration.cpp)

# Channel 事件管理测试
add_hyper_test(ChannelTests test_channel.cpp)

# ProtobufHandler 完整流程测试
add_hyper_test(ProtobufHandlerTests test_protobuf_handler.cpp)

# Connector 测试
add_hyper_test(ConnectorTests test_connector.cpp)

# TcpClient 测试
add_hyper_test(TcpClientTests test_tcpclient.cpp)
