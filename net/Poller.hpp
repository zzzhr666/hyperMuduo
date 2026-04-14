#pragma once
#include <chrono>
#include <unordered_map>
#include <vector>
#include "PollerBase.hpp"
struct pollfd;
namespace hyperMuduo::net {
    class Channel;
    class EventLoop;


    class Poller :public PollerBase{
    public:


        explicit Poller(EventLoop& loop);

        ~Poller() override = default;

        Poller(const Poller&) = delete;

        Poller& operator=(const Poller&) = delete;

        Poller(Poller&&) = delete;

        Poller& operator=(Poller&&) = delete;

        TimePoint poll(std::chrono::milliseconds timeout, ChannelList& active_channels) override;

        void updateChannel(Channel& channel) override;

        void removeChannel(Channel& channel) override;

    private:
        using PollFdList = std::vector<pollfd>;
        void fillActiveChannels(int num_events, ChannelList& active_channels);
    private:
        ChannelMap channels_;
        PollFdList pollFdList_;
    };
}
