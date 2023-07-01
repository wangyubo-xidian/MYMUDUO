#include "HttpServer.h"
#include "HttpRequest.h"
#include "HttpResponse.h"
#include "HttpContext.h"

#include <memory>

/**
 * 默认的http回调函数
 * 设置响应状态码，响应信息并关闭连接
 */
void defaultHttpCallback(const HttpRequest&, HttpResponse* resp)
{
    resp->setStatusCode(HttpResponse::k404NotFound);
    resp->setStatusMessage("Not Found");
    resp->setCloseConnection(true);
}

HttpServer::HttpServer(EventLoop *loop,
                      const InetAddress &listenAddr,
                      const std::string &name,
                      TcpServer::Option option)
  : server_(loop, listenAddr, name, option),
    httpCallback_(defaultHttpCallback)
{
    server_.setConnectionCallback(
        std::bind(&HttpServer::onConnection, this, std::placeholders::_1));
    server_.setMessageCallback(
        std::bind(&HttpServer::onMessage, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
    server_.setThreadNum(4);
}

void HttpServer::start()
{
    server_.start();
}

void HttpServer::onConnection(const TcpConnectionPtr& conn)
{
    if (conn->connected())
    {
        LOG_INFO("Connection UP");
    }
    else 
    {
        LOG_INFO("Connection DOWN");
    }
}

// 有消息到来时的业务处理
void HttpServer::onMessage(const TcpConnectionPtr& conn, Buffer* buf, Timestamp receiveTime)
{
    std::unique_ptr<HttpContext> context(new HttpContext);

    // 进行状态机解析
    // 错误则发送 BAD REQUEST 半关闭
    if (!context->parseRequest(buf, receiveTime))
    {
        conn->send("HTTP/1.1 400 Bad Request\r\n\r\n");
        conn->shutdown();
    }

    // 如果成功解析
    if (context->gotAll())
    {
        onRequest(conn, context->request());
        context->reset();
    }
}

void HttpServer::onRequest(const TcpConnectionPtr& conn, const HttpRequest& req)
{
    const std::string& connection = req.getHeader("Connection");

    // 判断长连接还是短连接
    bool close = connection == "close" ||
        (req.version() == HttpRequest::kHttp10 && connection != "Keep-Alive");
    // TODO:这里有问题，但是强制改写了
    close = true;
    // 响应信息
    HttpResponse response(close);
    // httpCallback_ 由用户传入，怎么写响应体由用户决定
    // 此处初始化了一些response的信息，比如响应码，回复OK
    httpCallback_(req, &response);
    Buffer buf;
    response.appendToBuffer(&buf);
    // TODO:需要重载 TcpConnection::send 使其可以接收一个缓冲区
    // conn->send(&buf);
    conn->send(std::string(buf.peek(), buf.readableBytes()));
    if (response.closeConnection())
    {
        conn->shutdown();
    }
}