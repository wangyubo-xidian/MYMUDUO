#pragma once

#include "noncopyable.h"
#include "Timestamp.h"

#include <functional>
#include <memory>

// 前置声明
class EventLoop;

/**
 * EventLoop Channel Poller之间的关系 => 在Reactor模型上对应的是 Demultiplex
 * Channel 理解为通道，封装了sockfd和其感兴趣的event，如EPOLLIN、EPOLLOUT事件，同时还绑定了poller返回的具体事件
 */
class Channel : noncopyable
{
public:
    using EventCallback = std::function<void()>;
    using ReadEventCallback = std::function<void(Timestamp)>;

    Channel(EventLoop* Loop, int fd);
    ~Channel();

    // fd得到poller通知以后处理事件
    void handleEvent(Timestamp receiveTime);

    // 设置回调函数对象
    void setReadCallback(ReadEventCallback cb) { readCallback_ = std::move(cb); }
    void setWriteCallback(EventCallback cb) { writeCallback_ = std::move(cb); }
    void setCloseCallback(EventCallback cb) { closeCallback_ = std::move(cb); }
    void setErrorCallback(EventCallback cb) { errorCallback_ = std::move(cb); }

    // 防止当Channel被手动remove掉时还在执行回调操作
    void tie(const std::shared_ptr<void>& obj);

    int fd() const { return fd_; }
    int events() const { return events_; }
    // set-revents由Poller使用
    void set_revents(int revt) { revents_ = revt; }

    // 设置fd相应的事件状态
    void enableReading() { events_ |= kReadEvent; update(); }
    void disableReading() { events_ &= ~kReadEvent; update(); }
    void enableWriting() { events_ |= kWriteEvent; update(); }
    void disableWriting() { events_ &= ~kWriteEvent; update(); }
    void disableAll() { events_ = kNoneEvent; update(); }

    // 返回fd当前的事件状态
    bool isWriting() const { return events_ & kWriteEvent; }
    bool isReading() const { return events_ & kReadEvent; }
    bool isNoneEvent() const { return events_ == kNoneEvent; }

    int index() { return index_; }
    void set_index(int idx) { index_ = idx; }

    // one loop per thread
    EventLoop* ownerLoop() { return loop_; }
    void remove();

private:

    void update();
    void handleEventWithGuard(Timestamp reveiveTime);

    static const int kNoneEvent;
    static const int kReadEvent;
    static const int kWriteEvent;

    EventLoop* loop_;     // 时间循环
    const int  fd_;       // Poller监听的对象
    int        events_;   // 注册fd感兴趣的事件
    int        revents_;  // poller返回的具体发生的事件
    int        index_;    // 表示Channel在Poller中的状态，初始化未-1

    std::weak_ptr<void> tie_;
    bool tied_;

    // 因为Channel通道里能够获知fd最终发生的具体事件revents，所以它负责调用具体事件的回调操作
    ReadEventCallback readCallback_;
    EventCallback writeCallback_;
    EventCallback closeCallback_;
    EventCallback errorCallback_;
};