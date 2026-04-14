#pragma once
#if defined(__linux__)
#include <unordered_map>
#include <vector>
#include "PollerBase.hpp"
struct epoll_event;
namespace hyperMuduo::net {
    class Epoller : public PollerBase {
    public:
        static constexpr int INVALID_EPOLL_FD = -1;
        explicit Epoller(EventLoop& loop);

        Epoller(const Epoller&) = delete;
        Epoller& operator=(const Epoller&) = delete;
        Epoller(Epoller&&) = delete;
        Epoller& operator=(Epoller&&) = delete;

        void removeChannel(Channel& channel) override;

        void updateChannel(Channel& channel) override;

        TimePoint poll(std::chrono::milliseconds timeout, ChannelList& active_channels) override;


        ~Epoller() override;
    private:
        void fillActiveChannels(int num_events,ChannelList& active_channel);


        int epoll_fd_;
        std::vector<epoll_event>events_;
        ChannelMap channel_map_;
    };

}
#endif
