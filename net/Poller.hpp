#pragma once
#include <chrono>
#include <unordered_map>
#include <vector>
struct pollfd;
namespace hyperMuduo::net {
    class Channel;
    class EventLoop;


    class Poller {
    public:
        using ChannelList = std::vector<Channel*>;
        using TimePoint = std::chrono::system_clock::time_point;

        explicit Poller(EventLoop& loop);

        ~Poller() = default;

        Poller(const Poller&) = delete;

        Poller& operator=(const Poller&) = delete;

        Poller(Poller&&) = delete;

        Poller& operator=(Poller&&) = delete;

        EventLoop& getLoop() const {
            return owner_loop_;
        }

        TimePoint poll(std::chrono::milliseconds timeout, ChannelList& active_channels);

        void updateChannel(Channel& channel);

        void removeChannel(Channel& channel);

        void assertInLoopThread();
    private:
        using PollFdList = std::vector<pollfd>;
        using ChannelMap = std::unordered_map<int, Channel*>;

        void fillActiveChannels(int num_events, ChannelList& active_channels);
    private:

        EventLoop& owner_loop_;
        ChannelMap channels_;
        PollFdList pollFdList_;
    };
}
