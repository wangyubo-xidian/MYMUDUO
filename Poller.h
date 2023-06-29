#pragma once

#include "noncopyable.h"

#include <vector>
#include <unordered_map>

class Channel;
class EventLoop;
class Timestamp;

// moduo库中多路事件分发器的核心IO复用模块
class Poller : noncopyable
{
public:
    using ChannelList = std::vector<Channel*>;

    Poller(EventLoop* loop);
    virtual ~Poller() = default;

    // 给所有IO复用保留统一的接口
    virtual Timestamp poll(int timeoutMs, ChannelList* activeChannels) = 0;
    virtual void updateChannel(Channel* channel) = 0;
    virtual void removeChannel(Channel* channel) = 0;

    // 判断阐述channel是否在当前Poller当中
    virtual bool hasChannel(Channel* channel) const;

    // EventLoop可以通过该接口获取默认的IO复用的具体实现
    static Poller* newDefaultPoller(EventLoop* loop);

protected:
    // map的key: sockfd  value: sockfd所属的channel通道类型
    using ChannelMap = std::unordered_map<int, Channel*>;
    ChannelMap channels_;
private:
    // 定义Poller所属的事件循环EventLoop
    EventLoop* ownerLoop_;
};