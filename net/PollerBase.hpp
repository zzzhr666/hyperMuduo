#pragma once

#include <chrono>
#include <vector>
#include "EventLoop.hpp"

namespace hyperMuduo::net {
    class Channel;

    class PollerBase {
    public:
        using ChannelList = std::vector<Channel*>;
        using TimePoint = std::chrono::system_clock::time_point;

        explicit PollerBase(EventLoop& loop)
            : owner_loop_(loop) {
        }

        virtual TimePoint poll(std::chrono::milliseconds timeout, ChannelList& active_channels) = 0;

        virtual void updateChannel(Channel& channel) = 0;

        virtual void removeChannel(Channel& channel) = 0;

        virtual EventLoop& getLoop() const {
            return owner_loop_;
        }


        virtual ~PollerBase() = default;

    protected:
        using ChannelMap = std::unordered_map<int, Channel*>;

        void assertInLoopThread() const {
            owner_loop_.assertInLoopThread();
        }

    private:
        EventLoop& owner_loop_;
    };

}
