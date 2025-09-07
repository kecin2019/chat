#pragma once

#include "json.hpp"
#include "usermodel.hpp"
#include "offlinemessagemodel.hpp"

#include <muduo/net/TcpConnection.h>
#include <unordered_map>
#include <functional>
#include <mutex>
using namespace std;
using namespace muduo;
using namespace muduo::net;
using json = nlohmann::json;

// 表示处理消息的事件回调方法类型
using MsgHandler = function<void(const TcpConnectionPtr &, json &, Timestamp time)>;

// 聊天服务器业务类
class ChatService
{
public:
    // 获取单例对象的接口函数
    static ChatService *instance();
    // 处理登录业务
    void login(const TcpConnectionPtr &conn, json &js, Timestamp time);
    // 处理注册业务
    void reg(const TcpConnectionPtr &conn, json &js, Timestamp time);
    // 一对一聊天业务
    void oneChat(const TcpConnectionPtr &conn, json &js, Timestamp time);
    // 获取消息对应的处理器
    MsgHandler getMsgHandler(int msgid);
    // 处理客户端异常退出
    void clientCloseException(const TcpConnectionPtr &conn);

private:
    ChatService();

    // 存储消息id和其对应的业务处理方法
    unordered_map<int, MsgHandler> msgHandlerMap_;
    // 存储在线用户的通信连接
    unordered_map<int, TcpConnectionPtr> userConnMap_;
    // 定义互斥锁, 保证userConnMap_的线程安全
    mutex connMutex_;
    // 数据操作类对象
    UserModel usermodel_;
    // 离线消息数据操作类对象
    OfflineMsgModel offlineMsgModel_;
};