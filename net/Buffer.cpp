#include "Buffer.hpp"
#include <spdlog/spdlog.h>

#include <algorithm>
#include <sys/uio.h>
#include <array>
#include <cerrno>
#include <unistd.h>

hyperMuduo::net::Buffer::Buffer(size_t initialize_size)
    : write_index_(CHEAP_PREPEND), read_index_(CHEAP_PREPEND) {
    buffer_.resize(CHEAP_PREPEND + initialize_size);
}

size_t hyperMuduo::net::Buffer::readableBytes() const {
    return write_index_ - read_index_;
}

size_t hyperMuduo::net::Buffer::writableBytes() const {
    return buffer_.size() - write_index_;
}

size_t hyperMuduo::net::Buffer::prependableBytes() const {
    return read_index_;
}

const char* hyperMuduo::net::Buffer::peek() const {
    return begin() + read_index_;
}

char* hyperMuduo::net::Buffer::begin() {
    return buffer_.data();
}

const char* hyperMuduo::net::Buffer::begin() const {
    return buffer_.data();
}

void hyperMuduo::net::Buffer::retrieve(size_t len) {
    if (len >= readableBytes()) {
        retrieveAll();
    } else {
        read_index_ += len;
    }
}

void hyperMuduo::net::Buffer::retrieveAll() {
    write_index_ = read_index_ = CHEAP_PREPEND;
}

std::string hyperMuduo::net::Buffer::retrieveAsString(size_t len) {
    std::string msg(peek(), len);
    retrieve(len);
    return msg;
}

std::string hyperMuduo::net::Buffer::retrieveAllAsString() {
    std::string msg(peek(), readableBytes());
    retrieveAll();
    return msg;
}

void hyperMuduo::net::Buffer::append(const char* data, size_t len) {
    ensureWritableBytes(len);
    std::copy(data, data + len, begin() + write_index_);
    write_index_ += len;
}

void hyperMuduo::net::Buffer::append(std::string_view msg) {
    append(msg.data(), msg.size());
}

void hyperMuduo::net::Buffer::ensureWritableBytes(size_t len) {
    if (writableBytes() >= len) {
        return;
    }
    makeSpace(len);
}

void hyperMuduo::net::Buffer::makeSpace(size_t len) {
    auto space_left = writableBytes() + (read_index_ - CHEAP_PREPEND);
    if (space_left >= len) {
        auto len_data = readableBytes();
        std::move(begin() + read_index_, begin() + read_index_ + readableBytes(), begin() + CHEAP_PREPEND);
        read_index_ = CHEAP_PREPEND;
        write_index_ = read_index_ + len_data;
    } else {
        buffer_.resize(write_index_ + len);
    }
}

void hyperMuduo::net::Buffer::prepend(const void* data, size_t len) {
    if (len > prependableBytes()) {
        SPDLOG_ERROR("Buffer::prepend invalid len: {}", len);
        return;
    }
    auto start = static_cast<const char*>(data);
    read_index_ -= len;
    std::copy(start, start + len, begin() + read_index_);
}

char* hyperMuduo::net::Buffer::beginWrite() {
    return buffer_.data() + write_index_;
}

const char* hyperMuduo::net::Buffer::beginWrite() const {
    return buffer_.data() + write_index_;
}

void hyperMuduo::net::Buffer::hasWritten(size_t len) {
    write_index_ += len;
}

void hyperMuduo::net::Buffer::swap(Buffer& other) {

    std::swap(other.buffer_, buffer_);
    std::swap(other.write_index_, write_index_);
    std::swap(other.read_index_, read_index_);

}

ssize_t hyperMuduo::net::Buffer::readFd(int fd, int* savedErrno) {
    std::array<char,65536> extra_arr;
    std::array<iovec,2> io_vec{
    {
    {begin() + write_index_, writableBytes()},
    {extra_arr.data(), 65536}
    }
    };
    ssize_t n_read = ::readv(fd, io_vec.data(), 2);
    if (n_read < 0) {
        *savedErrno = errno;
        SPDLOG_ERROR("Buffer::readFd error: {}", strerror(errno));
    } else {
        size_t writable = writableBytes();
        if (n_read <= writable) {
            write_index_ += n_read;
        } else {
            write_index_ = buffer_.size();
            append(extra_arr.data(), n_read - writable);
        }
    }
    return n_read;
}

ssize_t hyperMuduo::net::Buffer::writeFd(int fd, int* savedErrno) {
    ssize_t n_write = ::write(fd, peek(), readableBytes());
    if (n_write < 0) {
        *savedErrno = errno;
        if (errno != EWOULDBLOCK && errno != EAGAIN) {
            SPDLOG_ERROR("Buffer::writeFd error: {}", strerror(errno));
        }
    } else {
        retrieve(static_cast<size_t>(n_write));
    }
    return n_write;
}
