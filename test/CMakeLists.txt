find_package(GTest REQUIRED)

add_executable(HyperMuduoGTests
    test_buffer.cpp
    test_codec.cpp
    test_protobuf.cpp
    test_event_loop.cpp
    test_timer.cpp
    test_timerqueue.cpp
)

target_link_libraries(HyperMuduoGTests
    PRIVATE
    hyper_muduo
    GTest::gtest
    GTest::gtest_main
)

enable_testing()
add_test(NAME HyperMuduoGTests COMMAND HyperMuduoGTests)
