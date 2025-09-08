#pragma once

#include <muduo/net/TcpClient.h>
#include <muduo/net/EventLoop.h>
#include <muduo/net/InetAddress.h>
#include <muduo/net/TcpConnection.h>
#include <string>
#include <functional>
#include "json.hpp"

using namespace std;
using namespace muduo;
using namespace muduo::net;
using json = nlohmann::json;

// 消息处理回调类型
using MsgHandler = function<void(const json &js, const TcpConnectionPtr &conn)>;

class ChatClient
{
public:
    // 初始化客户端
    ChatClient(EventLoop *loop, const InetAddress &serverAddr);

    // 连接服务器
    void connect();

    // 断开连接
    void disconnect();

    // 发送消息
    void send(const string &msg);

    // 设置消息处理器
    void setMsgHandler(MsgHandler handler);

private:
    // 连接回调
    void onConnection(const TcpConnectionPtr &conn);

    // 消息接收回调
    void onMessage(const TcpConnectionPtr &conn, Buffer *buf, Timestamp time);

    TcpClient client_;      // muduo客户端对象
    EventLoop *loop_;       // 事件循环
    MsgHandler msgHandler_; // 消息处理器
};