#pragma once

#include <vector>
#include <string>
#include <sys/types.h>


namespace hyperMuduo::net {
    class Buffer {
    public:
        static constexpr size_t CHEAP_PREPEND = 8;
        static constexpr size_t INITIALIZE_SIZE = 1024;

        explicit Buffer(size_t initialize_size = INITIALIZE_SIZE);

        //游标状态
        [[nodiscard]] size_t readableBytes() const;

        [[nodiscard]]size_t writableBytes() const;

        [[nodiscard]] size_t prependableBytes() const;

        //读取数据
        [[nodiscard]] const char* peek() const;

        void retrieve(size_t len);

        void retrieveAll();

        std::string retrieveAsString(size_t len);

        std::string retrieveAllAsString();

        //写入数据
        void append(const char* data, size_t len);

        void append(std::string_view msg);

        void ensureWritableBytes(size_t len);

        void prepend(const void* data, size_t len);

        // 手动更新写指针
        char* beginWrite();

        const char* beginWrite() const;

        void hasWritten(size_t len);

        void swap(Buffer& other);

        //网络IO
        ssize_t readFd(int fd, int* savedErrno);

        ssize_t writeFd(int fd, int* savedErrno);

    private:
        char* begin();

        [[nodiscard]] const char* begin() const;

        void makeSpace(size_t len);

    private:
        std::vector<char> buffer_;
        size_t write_index_{};
        size_t read_index_{};
    };
}
