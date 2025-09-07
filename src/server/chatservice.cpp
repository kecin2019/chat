#include "chatservice.hpp"
#include "public.hpp"

#include <string>
#include <vector>
#include <muduo/base/Logging.h>

using namespace std;
using namespace placeholders;
using namespace muduo;

// 获取单例对象的接口函数
ChatService *ChatService::instance()
{
    static ChatService service;
    return &service;
}

// 注册消息以及对应的Handler回调操作
ChatService::ChatService()
{
    msgHandlerMap_.insert({LOGIN_MSG, std::bind(&ChatService::login, this, _1, _2, _3)});
    msgHandlerMap_.insert({REG_MSG, std::bind(&ChatService::reg, this, _1, _2, _3)});
    msgHandlerMap_.insert({ONE_CHAT_MSG, std::bind(&ChatService::oneChat, this, _1, _2, _3)});
}

// 获取消息对应的处理器
MsgHandler ChatService::getMsgHandler(int msgid)
{
    // 记录错误日志, msgid没有对应事件处理回调
    auto it = msgHandlerMap_.find(msgid);
    if (it == msgHandlerMap_.end())
    {
        return [=](const TcpConnectionPtr &, json &, Timestamp)
        {
            // 返回一个默认的处理器, 空操作
            LOG_ERROR << "msgid: " << msgid << " can not find handler!";
        };
    }
    else
    {
        LOG_INFO << "msgid: " << msgid << " find handler!";
        return msgHandlerMap_[msgid];
    }
}

// 处理登录业务 ORM框架 (Object Relational Mapping) 对象关系映射
// 业务层操作的都是对象 DAO框架 (Data Access Object) 数据访问对象
void ChatService::login(const TcpConnectionPtr &conn, json &js, Timestamp time)
{
    int id = js["id"].get<int>();
    string password = js["password"];

    User user = usermodel_.query(id);
    if (user.getId() == id && user.getPassword() == password)
    {
        if (user.getState() == "online")
        {
            // 该用户已经登录, 不允许重复登录
            json response;
            response["msgid"] = LOGIN_MSG_ACK;
            response["errno"] = 2;
            response["errmsg"] = "该账号已经登录";
            conn->send(response.dump());
        }
        else
        {
            // 登录成功, 记录用户连接信息
            {
                lock_guard<mutex> lock(connMutex_);
                userConnMap_.insert({id, conn});
            }

            // 登录成功, 更新用户状态信息 state offline -> online
            user.setState("online");
            usermodel_.updateState(user);

            json response;
            response["msgid"] = LOGIN_MSG_ACK;
            response["errno"] = 0;
            response["id"] = user.getId();
            response["name"] = user.getName();
            // 查询该用户是否有离线消息
            vector<string> vec = offlineMsgModel_.query(id);
            if (!vec.empty())
            {
                response["offlinemsg"] = vec;
                // 读取该用户的离线消息后, 把该用户的所有离线消息删除掉
                offlineMsgModel_.remove(id);
            }

            conn->send(response.dump());
        }
    }
    else
    {
        // 该用户不存在, 或者密码错误, 登录失败
        json response;
        response["msgid"] = LOGIN_MSG_ACK;
        response["errno"] = 1;
        response["errmsg"] = "用户名或密码错误";
        conn->send(response.dump());
    }
}

// 处理注册业务 name password
void ChatService::reg(const TcpConnectionPtr &conn, json &js, Timestamp time)
{
    string name = js["name"];
    string password = js["password"];

    User user;
    user.setName(name);
    user.setPassword(password);
    bool state = usermodel_.insert(user);
    if (state)
    {
        // 注册成功
        json response;
        response["msgid"] = REG_MSG_ACK;
        response["id"] = user.getId();
        response["errno"] = 0;
        conn->send(response.dump());
    }
    else
    {
        // 注册失败
        json response;
        response["msgid"] = REG_MSG_ACK;
        response["errno"] = 1;
        response["errmsg"] = "注册失败";
        conn->send(response.dump());
    }
}

void ChatService::clientCloseException(const TcpConnectionPtr &conn)
{
    User user;
    {
        lock_guard<mutex> lock(connMutex_);
        for (auto it = userConnMap_.begin(); it != userConnMap_.end(); ++it)
        {
            if (it->second == conn)
            {
                // 从map表删除用户的连接信息
                user.setId(it->first);
                userConnMap_.erase(it);
                break;
            }
        }
    }

    // 更新用户状态信息 state online -> offline
    if (user.getId() != -1)
    {
        user.setState("offline");
        usermodel_.updateState(user);
    }
}

void ChatService::oneChat(const TcpConnectionPtr &conn, json &js, Timestamp time)
{
    int toid = js["to"].get<int>();

    {
        lock_guard<mutex> lock(connMutex_);
        auto it = userConnMap_.find(toid);
        if (it != userConnMap_.end())
        {
            // toid在线, 发送消息 服务器主动推送消息给toid用户
            it->second->send(js.dump());
            return;
        }
    }
    // toid不在线, 存储离线消息
    offlineMsgModel_.insert(toid, js.dump());
}