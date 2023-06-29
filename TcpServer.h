#pragma once

/**
 * 用户使用muduo编写服务器程序
 */
#include "noncopyable.h"
#include "EventLoop.h"
#include "Acceptor.h"
#include "InetAddress.h"
#include "EventLoopThreadPoool.h"
#include "Callbacks.h"
#include "TcpConnection.h"

#include <functional>
#include <string>
#include <memory>
#include <atomic>
#include <unordered_map>

// 对外的服务器编程使用的类
class TcpServer : noncopyable
{
public:
    using ThreadInitCallback = std::function<void(EventLoop*)>;
    
    enum Option
    {
        kNoReusePort,
        kReusePort
    };

    TcpServer(EventLoop* loop, const InetAddress& listenAddr, const std::string& nameArg, Option option = kNoReusePort);
    ~TcpServer();
    
    void setThreadInitCallback(const ThreadInitCallback& cb) { threadInitCallback_ = cb; }
    void setConnectionCallback(const ConnectionCallback& cb) { connectionCallback_ = cb; }
    void setMessageCallback(const MessageCallback& cb) { messageCallback_ = cb; }
    void setWriteCompleteCallback(const WriteCompleteCallback& cb) { writeCompleteCallback_ = cb; }

    // 设置subLoop的个数
    void setThreadNum(int numThreads);
    
    // 开启服务器监听
    void start();

private:
    /**
     * 根据轮询算法选择一个subloop并将其唤醒
     * 把当前connfd封装成channel分发给subloop
     * 主线程的mainLoop调用newConnection选择一个ioLoop，判断ioLoop是否在自己的线程，是--runInLoop，否--queueInLoop 
     */
    void newConnection(int sockfd, const InetAddress& peerAddr);

    void removeConnection(const TcpConnectionPtr& conn);

    void removeConnectionInLoop(const TcpConnectionPtr& conn);

    using ConnectionMap = std::unordered_map<std::string, TcpConnectionPtr>;

    EventLoop* loop_;   // baseloop 用户定义的loop
    const std::string ipPort_;
    const std::string name_;

    std::unique_ptr<Acceptor> acceptor_;   // 运行在mainLoop，监听新连接的事件

    std::shared_ptr<EventLoopThreadPool> threadPool_;   // one loop per thread

    ConnectionCallback connectionCallback_;         // 有新连接时的回调
    MessageCallback messageCallback_;             // 有读写消息的回调
    WriteCompleteCallback writeCompleteCallback_;   // 消息发送完成后的回调

    ThreadInitCallback threadInitCallback_;         // loop线程初始化的回调
    std::atomic_int started_;

    int nextConnId_;
    ConnectionMap connections_;   // 保存所有的连接
};