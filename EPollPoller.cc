#include "EPollPoller.h"
#include "Logger.h"
#include "Channel.h"

#include <errno.h>
#include <unistd.h>
#include <string.h>

// Channel未添加到Poller中
const int kNew = -1;   // Channel的成员index_ = -1，index_表示Channel在Poller中的状态
// Channel已添加到Poller中
const int kAdded = 1;
// Channel从Poller中删除（依然保留在Channels_中）
const int kDeleted = 2;

EPollPoller::EPollPoller(EventLoop *loop)
    : Poller(loop),
      epollfd_(::epoll_create1(EPOLL_CLOEXEC)),
      events_(kInitEventListSize)  
{
    if (epollfd_ < 0)
    {
        LOG_FATAL("epoll_create error:%d \n", errno);
    }
}

EPollPoller::~EPollPoller()
{
    ::close(epollfd_);
}

// epoll_wait
Timestamp EPollPoller::poll(int timeoutMs, ChannelList* activeChannels)
{
    // 实际上应该用LOG_DEBUG输出日志更为合理
    LOG_INFO("func=%s => fd total count:%lu\n", __FUNCTION__, channels_.size());

    int numEvents = ::epoll_wait(epollfd_, &*events_.begin(), static_cast<int>(events_.size()), timeoutMs);
    // 多线程
    int saveErrno = errno;
    Timestamp now(Timestamp::now());

    if (numEvents > 0)
    {
        LOG_INFO("%d events happened\n", numEvents);
        fillActiveChannels(numEvents, activeChannels);
        // events_两倍扩容
        if (numEvents == events_.size())
        {
            events_.resize(events_.size() * 2);
        }
    }
    else if (numEvents == 0)
    {
        LOG_DEBUG("%s timeout!\n", __FUNCTION__);
    }
    else
    {
        if(saveErrno != EINTR)  // EINTR 外部中断
        {
            errno = saveErrno;
            LOG_ERROR("EPollPoller::poll() error!");
        }
    }
    return now;
}

// Channel update remove => EventLoop updateChannel removeChannel => Poller
/**
 *             EventLoop
 *    ChannelList      Poller
 *                    ChannelMap <fd, channel*>
 */
void EPollPoller::updateChannel(Channel* channel)
{
    const int index = channel->index();
    LOG_INFO("func=%s fd=%d events=%d index=%d\n", __FUNCTION__, channel->fd(), channel->events(), index);

    if (index == kNew || index == kDeleted)
    {
        if (index == kNew)
        {
            int fd = channel->fd();
            channels_[fd] = channel;
        }

        channel->set_index(kAdded);
        update(EPOLL_CTL_ADD, channel);
    }
    else  // Channel已经在Poller中注册过了
    {
        int fd = channel->fd();
        if (channel->isNoneEvent())
        {
            update(EPOLL_CTL_DEL, channel);
            channel->set_index(kDeleted);
        }
        else
        {
            update(EPOLL_CTL_MOD, channel);
        }
    }
}

//  // 从Poller和ChannelMap中删除Channel   
void EPollPoller::removeChannel(Channel* channel)
{   
    int fd = channel->fd();
    channels_.erase(fd);

    LOG_INFO("func=%s fd=%d\n", __FUNCTION__, fd);

    int index = channel->index();
    if (index == kAdded)
    {
        update(EPOLL_CTL_DEL, channel);
    }
    channel->set_index(kNew);
}

// 填写活跃的连接
void EPollPoller::fillActiveChannels(int numEvents, ChannelList* activeChannels) const
{
    for (int i = 0; i < numEvents; ++i)
    {
        Channel* channel = static_cast<Channel*>(events_[i].data.ptr);
        channel->set_revents(events_[i].events);
        // EventLoop拿到了它的Poller给它返回的所有发生事件的channel列表了
        activeChannels->push_back(channel); 
    }
}
    
// 更新channel通道 epoll_ctl ADD/MOD/DEL
void EPollPoller::update(int operation, Channel* channel)
{
    epoll_event event;
    memset(&event, 0, sizeof(event));
    event.events = channel->events();
    event.data.ptr = channel;
    int fd = channel->fd();

    if (::epoll_ctl(epollfd_, operation, fd, &event) < 0)
    {
        if (operation == EPOLL_CTL_DEL)
        {
            LOG_ERROR("epoll_ctl del error:%d\n", errno);
        }
        else
        {
            LOG_FATAL("epoll_ctl add/mod error:%d\n", errno);
        }
    }
}