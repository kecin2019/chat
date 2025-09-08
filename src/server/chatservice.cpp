#include "chatservice.hpp"
#include "public.hpp"

#include <string>
#include <vector>
#include <map>
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
    msgHandlerMap_.insert({ADD_FRIEND_MSG, std::bind(&ChatService::addFriend, this, _1, _2, _3)});
    msgHandlerMap_.insert({DEL_FRIEND_MSG, std::bind(&ChatService::delFriend, this, _1, _2, _3)});
    msgHandlerMap_.insert({CREATE_GROUP_MSG, std::bind(&ChatService::createGroup, this, _1, _2, _3)});
    msgHandlerMap_.insert({ADD_GROUP_MSG, std::bind(&ChatService::addGroup, this, _1, _2, _3)});
    msgHandlerMap_.insert({QUIT_GROUP_MSG, std::bind(&ChatService::quitGroup, this, _1, _2, _3)});
    msgHandlerMap_.insert({GROUP_CHAT_MSG, std::bind(&ChatService::groupChat, this, _1, _2, _3)});
    msgHandlerMap_.insert({LOGIN_OUT_MSG, std::bind(&ChatService::loginOut, this, _1, _2, _3)});

    // 连接redis服务器
    if (redis_.connect())
    {
        // 设置上报消息的回调
        redis_.init_notify_handler(std::bind(&ChatService::handleRedisSubscribeMessage, this, _1, _2));
    }
}

// 服务器异常, 业务重置方法
void ChatService::reset()
{
    // 把online状态的用户状态改为offline
    usermodel_.resetState();
}

// 添加好友业务 msgid id frendid
void ChatService::addFriend(const TcpConnectionPtr &conn, json &js, Timestamp time)
{
    int userid = js["id"].get<int>();
    int friendid = js["friendid"].get<int>();

    // 存储好友信息
    friendmodel_.addFriend(userid, friendid);
}

// 删除好友业务
void ChatService::delFriend(const TcpConnectionPtr &conn, json &js, Timestamp time)
{
    int userid = js["id"].get<int>();
    int friendid = js["friendid"].get<int>();

    // 删除好友信息
    friendmodel_.delFriend(userid, friendid);
}

// 创建群组业务
void ChatService::createGroup(const TcpConnectionPtr &conn, json &js, Timestamp time)
{
    int userid = js["id"].get<int>();
    string groupname = js["groupname"];
    string groupdesc = js["groupdesc"];

    // 存储新创建的群组信息
    Group group(-1, groupname, groupdesc);
    if (groupmodel_.createGroup(group))
    {
        // 存储群组创建人信息
        groupmodel_.joinGroup(userid, group.getId(), "creator");
    }
}

// 加入群组业务
void ChatService::addGroup(const TcpConnectionPtr &conn, json &js, Timestamp time)
{
    int userid = js["id"].get<int>();
    int groupid = js["groupid"].get<int>();
    groupmodel_.joinGroup(userid, groupid, "normal");
}

// 退出群组业务
void ChatService::quitGroup(const TcpConnectionPtr &conn, json &js, Timestamp time)
{
    int userid = js["id"].get<int>();
    int groupid = js["groupid"].get<int>();
    groupmodel_.quitGroup(userid, groupid);
}

// 群组聊天业务
void ChatService::groupChat(const TcpConnectionPtr &conn, json &js, Timestamp time)
{
    int userid = js["id"].get<int>();
    int groupid = js["groupid"].get<int>();
    vector<int> useridVec = groupmodel_.queryGroupUsers(userid, groupid);
    {
        lock_guard<mutex> lock(connMutex_);
        for (int id : useridVec)
        {
            auto it = userConnMap_.find(id);
            if (it != userConnMap_.end())
            {
                // 转发群消息
                it->second->send(js.dump());
            }
            else
            {
                // 查询用户是否在线
                User user = usermodel_.query(id);
                if (user.getState() == "online")
                {
                    redis_.publish(id, js.dump());
                }
                else
                {
                    // 存储离线群消息
                    offlineMsgModel_.insert(id, js.dump());
                }
            }
        }
    }
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

            // 登录成功, 向redis订阅该用户的频道
            redis_.subscribe(id);

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
            // 查询该用户的好友信息并返回
            vector<User> userVec = friendmodel_.queryFriends(id);
            if (!userVec.empty())
            {
                vector<string> vec2;
                for (auto &user : userVec)
                {
                    json js;
                    js["id"] = user.getId();
                    js["name"] = user.getName();
                    js["state"] = user.getState();
                    vec2.push_back(js.dump());
                }
                response["friends"] = vec2;
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

// 处理注销业务
void ChatService::loginOut(const TcpConnectionPtr &conn, json &js, Timestamp time)
{
    int userid = js["id"].get<int>();
    {
        lock_guard<mutex> lock(connMutex_);
        auto it = userConnMap_.find(userid);
        if (it != userConnMap_.end())
        {
            // 从map表删除用户的连接信息
            userConnMap_.erase(it);
        }

        // 向redis取消订阅该用户的频道
        redis_.unsubscribe(userid);

        // 更新用户状态信息 state online -> offline
        User user;
        user.setId(userid);
        user.setState("offline");
        usermodel_.updateState(user);
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

    // 向redis取消订阅该用户的频道
    redis_.unsubscribe(user.getId());

    // 更新用户状态信息 state online -> offline
    if (user.getId() != -1)
    {
        user.setState("offline");
        usermodel_.updateState(user);
    }
}

// 一对一聊天业务
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
    // 查询toid是否在线
    User user = usermodel_.query(toid);
    if (user.getState() == "online")
    {
        redis_.publish(toid, js.dump());
    }
    else
    {
        // toid不在线, 存储离线消息
        offlineMsgModel_.insert(toid, js.dump());
    }
}

void ChatService::handleRedisSubscribeMessage(int userid, string message)
{
    {
        lock_guard<mutex> lock(connMutex_);
        auto it = userConnMap_.find(userid);
        if (it != userConnMap_.end())
        {
            it->second->send(message);
        }
        else
        {
            // 用户不在线, 把消息存储到离线消息表中
            offlineMsgModel_.insert(userid, message);
        }
    }
}