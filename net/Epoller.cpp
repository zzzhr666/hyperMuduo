#include "Epoller.hpp"
#include <sys/epoll.h>
#include <spdlog/spdlog.h>

namespace {
    int createEpollFd() {
        int ret = ::epoll_create1(EPOLL_CLOEXEC);
        if (ret == -1) {
            SPDLOG_CRITICAL("Unable to create epoll fd,error message:{}", std::system_category().message(errno));
            std::abort();
        }
        return ret;
    }

    uint32_t pollToEpollEvents(int poll_event) {
        uint32_t events = 0;
        if (poll_event & POLLIN) {
            events |= EPOLLIN;
        }
        if (poll_event & POLLOUT) {
            events |= EPOLLOUT;
        }
        if (poll_event & POLLHUP) {
            events |= EPOLLHUP;
        }
        if (poll_event & POLLERR) {
            events |= EPOLLERR;
        }
        if (poll_event & POLLPRI) {
            events |= EPOLLPRI;
        }
        events |= EPOLLRDHUP;
        return events;
    }

    int setPollEvents(uint32_t epoll_events) {
        int poll_events{};
        if (epoll_events & EPOLLIN) {
            poll_events |= POLLIN;
        }
        if (epoll_events & EPOLLOUT) {
            poll_events |= POLLOUT;
        }
        if (epoll_events & EPOLLHUP) {
            poll_events |= POLLHUP;
        }
        if (epoll_events & EPOLLERR) {
            poll_events |= POLLERR;
        }
        if (epoll_events & EPOLLPRI) {
            poll_events |= POLLPRI;
        }
        if (epoll_events & EPOLLRDHUP) {
            poll_events |= POLLHUP;
        }
        return poll_events;
    }


}

hyperMuduo::net::Epoller::Epoller(EventLoop& loop)
    : PollerBase(loop), epoll_fd_(createEpollFd()) {
    events_.resize(16);
}

void hyperMuduo::net::Epoller::removeChannel(Channel& channel) {
    assertInLoopThread();
    if (auto it = channel_map_.find(channel.getFd()); it != channel_map_.end()) {
        if (::epoll_ctl(epoll_fd_, EPOLL_CTL_DEL, channel.getFd(), nullptr) == -1) {
            SPDLOG_ERROR("epoll_ctl EPOLL_CTL_DEL failed , fd = {},error message={}", channel.getFd(),
                         std::system_category().message(errno));
        }
        channel_map_.erase(it);
        return;
    }
    SPDLOG_ERROR("channel (fd = {}) not found.", channel.getFd());
}

void hyperMuduo::net::Epoller::updateChannel(Channel& channel) {
    assertInLoopThread();
    SPDLOG_TRACE("fd = {},event = {}", channel.getFd(), channel.getEvents());
    epoll_event event{};
    event.data.ptr = &channel;
    event.events = pollToEpollEvents(channel.getEvents());
    if (auto option = channel_map_.contains(channel.getFd()) ? EPOLL_CTL_MOD : EPOLL_CTL_ADD;
        ::epoll_ctl(epoll_fd_, option, channel.getFd(), &event) < 0) {
        SPDLOG_ERROR("Failed to control epoll,op={},fd={},error message = {}", option, channel.getFd(),
                     std::system_category().message(errno));
        return;
    }
    channel_map_[channel.getFd()] = &channel;
}

hyperMuduo::net::PollerBase::TimePoint hyperMuduo::net::Epoller::poll(std::chrono::milliseconds timeout,
                                                                      ChannelList& active_channels) {
    int num_event = ::epoll_wait(epoll_fd_, events_.data(), events_.size(), timeout.count());
    auto now = std::chrono::system_clock::now();
    if (num_event == events_.size()) {
        events_.resize(events_.size() * 2);
    }
    if (num_event < 0) {
        SPDLOG_ERROR("epoll_wait failed...");
    } else if (num_event == 0) {
        SPDLOG_TRACE("Timeout...");
    } else {
        fillActiveChannels(num_event, active_channels);
    }
    return now;
}

hyperMuduo::net::Epoller::~Epoller() {
    ::close(epoll_fd_);
}

void hyperMuduo::net::Epoller::fillActiveChannels(int num_events, ChannelList& active_channel) {
    for (int i = 0; i < num_events; ++i) {
        auto channel = static_cast<Channel*>(events_[i].data.ptr);
        if (!channel) {
            continue;
        }
        channel->setReturnEvents(setPollEvents(events_[i].events));
        active_channel.push_back(channel);
    }
}
