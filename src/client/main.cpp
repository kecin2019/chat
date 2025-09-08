#include "chatclient.hpp"
#include "public.hpp"
#include <iostream>
#include <thread>
#include <mutex>
#include <condition_variable>

using namespace std;

// 全局变量，记录当前登录用户的ID和名称
int g_userId = -1;
string g_userName = "";
mutex g_mutex;
condition_variable g_cond;
bool g_isLoginSuccess = false;

// 显示菜单
void showMenu()
{
    cout << "========================" << endl;
    cout << "1. 登录" << endl;
    cout << "2. 注册" << endl;
    cout << "3. 一对一聊天" << endl;
    cout << "4. 添加好友" << endl;
    cout << "5. 删除好友" << endl;
    cout << "6. 创建群组" << endl;
    cout << "7. 加入群组" << endl;
    cout << "8. 退出群组" << endl;
    cout << "9. 群聊" << endl;
    cout << "0. 退出" << endl;
    cout << "========================" << endl;
}

// 处理服务器返回的消息
void handleServerMsg(const json &js, const TcpConnectionPtr &conn)
{
    if (js["msgid"].get<int>() == LOGIN_MSG_ACK)
    {
        if (js["errno"].get<int>() == 0)
        {
            // 登录成功
            g_userId = js["id"].get<int>();
            g_userName = js["name"];
            g_isLoginSuccess = true;

            cout << "登录成功! 欢迎您，" << g_userName << endl;

            // 打印离线消息（如果有）
            if (js.find("offlinemsg") != js.end())
            {
                vector<string> offlinemsg = js["offlinemsg"];
                for (const string &msg : offlinemsg)
                {
                    json msgJs = json::parse(msg);
                    cout << "离线消息: " << msgJs["from"] << "说: " << msgJs["msg"] << endl;
                }
            }

            // 打印好友列表
            if (js.find("friends") != js.end())
            {
                vector<string> friends = js["friends"];
                cout << "您的好友列表: " << endl;
                for (const string &f : friends)
                {
                    json fJs = json::parse(f);
                    cout << fJs["id"] << " " << fJs["name"] << " 状态: " << fJs["state"] << endl;
                }
            }
        }
        else
        {
            // 登录失败
            cout << "登录失败: " << js["errmsg"] << endl;
        }
        g_cond.notify_one();
    }
    else if (js["msgid"].get<int>() == REG_MSG_ACK)
    {
        if (js["errno"].get<int>() == 0)
        {
            cout << "注册成功! 您的用户ID是: " << js["id"] << endl;
        }
        else
        {
            cout << "注册失败: " << js["errmsg"] << endl;
        }
        g_cond.notify_one();
    }
    else if (js["msgid"].get<int>() == ONE_CHAT_MSG)
    {
        cout << js["from"] << "说: " << js["msg"] << endl;
    }
    else if (js["msgid"].get<int>() == GROUP_CHAT_MSG)
    {
        cout << "群[" << js["groupid"] << "]中" << js["from"] << "说: " << js["msg"] << endl;
    }
    else
    {
        cout << "收到未知类型消息" << endl;
    }
}

// 处理用户输入
void inputThread(ChatClient &client)
{
    showMenu();
    int op;
    while (cin >> op)
    {
        switch (op)
        {
        case 1: // 登录
        {
            int id;
            string pwd;
            cout << "请输入用户ID: ";
            cin >> id;
            cout << "请输入密码: ";
            cin >> pwd;

            json js;
            js["msgid"] = LOGIN_MSG;
            js["id"] = id;
            js["password"] = pwd;
            client.send(js.dump());

            // 等待登录结果
            unique_lock<mutex> lock(g_mutex);
            g_cond.wait(lock);
            break;
        }
        case 2: // 注册
        {
            string name, pwd;
            cout << "请输入用户名: ";
            cin >> name;
            cout << "请输入密码: ";
            cin >> pwd;

            json js;
            js["msgid"] = REG_MSG;
            js["name"] = name;
            js["password"] = pwd;
            client.send(js.dump());

            // 等待注册结果
            unique_lock<mutex> lock(g_mutex);
            g_cond.wait(lock);
            break;
        }
        case 3: // 一对一聊天
        {
            if (g_userId == -1)
            {
                cout << "请先登录!" << endl;
                break;
            }
            int toId;
            string msg;
            cout << "请输入聊天对象ID: ";
            cin >> toId;
            cin.ignore(); // 忽略换行符
            cout << "请输入消息内容: ";
            getline(cin, msg);

            json js;
            js["msgid"] = ONE_CHAT_MSG;
            js["id"] = g_userId;
            js["to"] = toId;
            js["msg"] = msg;
            js["from"] = g_userName;
            client.send(js.dump());
            break;
        }
        case 4: // 添加好友
        {
            if (g_userId == -1)
            {
                cout << "请先登录!" << endl;
                break;
            }
            int friendId;
            cout << "请输入要添加的好友ID: ";
            cin >> friendId;

            json js;
            js["msgid"] = ADD_FRIEND_MSG;
            js["id"] = g_userId;
            js["friendid"] = friendId;
            client.send(js.dump());
            cout << "添加好友请求已发送" << endl;
            break;
        }
        case 5: // 删除好友
        {
            if (g_userId == -1)
            {
                cout << "请先登录!" << endl;
                break;
            }
            int friendId;
            cout << "请输入要删除的好友ID: ";
            cin >> friendId;

            json js;
            js["msgid"] = DEL_FRIEND_MSG;
            js["id"] = g_userId;
            js["friendid"] = friendId;
            client.send(js.dump());
            cout << "已删除好友" << endl;
            break;
        }
        case 6: // 创建群组
        {
            if (g_userId == -1)
            {
                cout << "请先登录!" << endl;
                break;
            }
            string groupName, groupDesc;
            cout << "请输入群组名称: ";
            cin >> groupName;
            cin.ignore();
            cout << "请输入群组描述: ";
            getline(cin, groupDesc);

            json js;
            js["msgid"] = CREATE_GROUP_MSG;
            js["id"] = g_userId;
            js["groupname"] = groupName;
            js["groupdesc"] = groupDesc;
            client.send(js.dump());
            cout << "群组创建请求已发送" << endl;
            break;
        }
        case 7: // 加入群组
        {
            if (g_userId == -1)
            {
                cout << "请先登录!" << endl;
                break;
            }
            int groupId;
            cout << "请输入要加入的群组ID: ";
            cin >> groupId;

            json js;
            js["msgid"] = ADD_GROUP_MSG;
            js["id"] = g_userId;
            js["groupid"] = groupId;
            client.send(js.dump());
            cout << "加入群组请求已发送" << endl;
            break;
        }
        case 8: // 退出群组
        {
            if (g_userId == -1)
            {
                cout << "请先登录!" << endl;
                break;
            }
            int groupId;
            cout << "请输入要退出的群组ID: ";
            cin >> groupId;

            json js;
            js["msgid"] = QUIT_GROUP_MSG;
            js["id"] = g_userId;
            js["groupid"] = groupId;
            client.send(js.dump());
            cout << "已退出群组" << endl;
            break;
        }
        case 9: // 群聊
        {
            if (g_userId == -1)
            {
                cout << "请先登录!" << endl;
                break;
            }
            int groupId;
            string msg;
            cout << "请输入群组ID: ";
            cin >> groupId;
            cin.ignore();
            cout << "请输入消息内容: ";
            getline(cin, msg);

            json js;
            js["msgid"] = GROUP_CHAT_MSG;
            js["id"] = g_userId;
            js["groupid"] = groupId;
            js["msg"] = msg;
            js["from"] = g_userName;
            client.send(js.dump());
            break;
        }
        case 0: // 退出
            client.disconnect();
            return;
        default:
            cout << "无效操作，请重新输入!" << endl;
            break;
        }
        showMenu();
    }
}

int main(int argc, char *argv[])
{
    EventLoop loop;
    InetAddress serverAddr("127.0.0.1", 8001);
    ChatClient client(&loop, serverAddr);

    // 设置消息处理器
    client.setMsgHandler(handleServerMsg);

    // 连接服务器
    client.connect();

    // 创建输入线程
    thread t(inputThread, ref(client));
    t.detach();

    // 启动事件循环
    loop.loop();

    return 0;
}