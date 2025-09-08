#include "chatclient.hpp"
#include "public.hpp"
#include <muduo/base/Logging.h>

ChatClient::ChatClient(EventLoop *loop, const InetAddress &serverAddr)
    : client_(loop, serverAddr, "ChatClient"), loop_(loop)
{
    // 注册连接回调
    client_.setConnectionCallback(
        std::bind(&ChatClient::onConnection, this, _1));

    // 注册消息接收回调
    client_.setMessageCallback(
        std::bind(&ChatClient::onMessage, this, _1, _2, _3));
}

void ChatClient::connect()
{
    client_.connect();
}

void ChatClient::disconnect()
{
    client_.disconnect();
}

void ChatClient::send(const string &msg)
{
    TcpConnectionPtr conn = client_.connection();
    if (conn && conn->connected())
    {
        conn->send(msg);
    }
}

void ChatClient::setMsgHandler(MsgHandler handler)
{
    msgHandler_ = handler;
}

void ChatClient::onConnection(const TcpConnectionPtr &conn)
{
    if (!conn->connected())
    {
        // 连接断开
        LOG_INFO << "Server disconnected!";
        loop_->quit(); // 退出事件循环
    }
}

void ChatClient::onMessage(const TcpConnectionPtr &conn, Buffer *buf, Timestamp time)
{
    string msg = buf->retrieveAllAsString();
    // 解析JSON
    json js = json::parse(msg);
    if (msgHandler_)
    {
        // 调用注册的消息处理器
        msgHandler_(js, conn);
    }
}