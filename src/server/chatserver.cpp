#include "chatserver.hpp"
#include "chatservice.hpp"
#include "json.hpp"

#include <functional>
#include <string>

using namespace std;
using namespace placeholders;
using json = nlohmann::json;

// 初始化聊天服务器对象
ChatServer::ChatServer(EventLoop *loop,
                       const InetAddress &listenAddr,
                       const string &nameArg)
    : server_(loop, listenAddr, nameArg),
      loop_(loop)
{
    // 注册链接回调
    server_.setConnectionCallback(std::bind(&ChatServer::onConnection, this, _1));

    // 注册消息回调
    server_.setMessageCallback(std::bind(&ChatServer::onMessage, this, _1, _2, _3));

    // 设置线程数量
    server_.setThreadNum(4);
}

// 启动服务
void ChatServer::start()
{
    server_.start();
}

// 上报链接相关信息的回调函数
void ChatServer::onConnection(const TcpConnectionPtr &conn)
{
    // 客户端断开链接
    if (!conn->connected())
    {
        ChatService::instance()->clientCloseException(conn);
        conn->shutdown();
    }
}

// 上报读写事件相关信息的回调函数
void ChatServer::onMessage(const TcpConnectionPtr &conn,
                           Buffer *buffer,
                           Timestamp time)
{
    string buf = buffer->retrieveAllAsString();
    // 数据的反序列化
    json js = json::parse(buf);
    // 达到的目的: 完全解耦网络模块的代码和业务模块的代码
    // 通过 js["msgid"] 获取 => 业务handler => conn js time
    // 面向接口的编程(在C++中, 接口就是虚基类), 或者使用回调函数
    auto msgHandler = ChatService::instance()->getMsgHandler(js["msgid"].get<int>());
    // 回调消息绑定好的事件处理器, 来执行相应的业务处理
    msgHandler(conn, js, time);
}