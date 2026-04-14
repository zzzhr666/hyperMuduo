#include <sys/poll.h>
#include <spdlog/spdlog.h>
#include "Poller.hpp"

#include "Channel.hpp"
#include "EventLoop.hpp"

hyperMuduo::net::Poller::Poller(EventLoop& loop)
    :PollerBase(loop) {
}


hyperMuduo::net::Poller::TimePoint hyperMuduo::net::Poller::poll(std::chrono::milliseconds timeout,
                                                                 ChannelList& active_channels) {
    int num_events = ::poll(pollFdList_.data(), pollFdList_.size(), timeout.count());
    auto now = std::chrono::system_clock::now();
    if (num_events < 0) {
        SPDLOG_ERROR("Poller::poll() failed.");
    } else if (num_events == 0) {
        SPDLOG_TRACE("Nothing happened.");
    } else {
        SPDLOG_TRACE("{} events happened", num_events);
        fillActiveChannels(num_events, active_channels);
    }
    return now;
}

void hyperMuduo::net::Poller::updateChannel(Channel& channel) {
    assertInLoopThread();
    SPDLOG_TRACE("fd = {},event = {}", channel->getFd(), channel->getEvents());
    if (channel.index() == Channel::NOT_FILL) {
        pollFdList_.emplace_back(channel.getFd(), static_cast<short>(channel.getEvents()), 0);
        int index = pollFdList_.size() - 1;
        channel.setIndex(index);
        channels_.insert_or_assign(channel.getFd(), &channel);
    } else {
        int index = channel.index();
        pollFdList_[index].events = channel.getEvents();
        pollFdList_[index].revents = 0;
        if (channel.isNoneEvent()) {
            pollFdList_[index].fd = ~channel.getFd();
        } else {
            pollFdList_[index].fd = channel.getFd();
        }
    }
}

void hyperMuduo::net::Poller::removeChannel(Channel& channel) {
    assertInLoopThread();
    int index = channel.index();
    if (index < 0 || index >= pollFdList_.size()) {
        return;
    }
    if (index < pollFdList_.size() - 1) {
        std::swap(pollFdList_[index], pollFdList_.back());
        int fd = pollFdList_[index].fd;
        if (fd < 0) {
            fd = ~fd;
        }
        if (auto it = channels_.find(fd); it != channels_.end()) {
            it->second->setIndex(index);
        }

    }
    pollFdList_.pop_back();
    channels_.erase(channel.getFd());
    channel.setIndex(Channel::NOT_FILL);

}


void hyperMuduo::net::Poller::fillActiveChannels(int num_events, ChannelList& active_channels) {
    for (auto it = pollFdList_.begin(); it != pollFdList_.end() && num_events > 0; ++it) {
        if (it->revents > 0) {
            --num_events;
            if (auto fd2channel = channels_.find(it->fd); fd2channel != channels_.end()) {
                auto* channel = fd2channel->second;
                if (channel->getFd() == it->fd) {
                    channel->setReturnEvents(it->revents);
                    active_channels.push_back(channel);
                }
            }
        }
    }
}
