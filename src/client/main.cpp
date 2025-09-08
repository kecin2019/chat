#include "json.hpp"
#include "group.hpp"
#include "user.hpp"
#include "public.hpp"

#include <iostream>
#include <thread>
#include <string>
#include <vector>
#include <chrono>
#include <ctime>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>

using namespace std;
using json = nlohmann::json;

// 记录当前系统登录的用户信息
User g_currentUser;
// 记录当前登录用户的好友列表信息
vector<User> g_currentUserFriendList;
// 记录当前登录用户的群组列表信息
vector<Group> g_currentUserGroupList;
// 显示当前登录成功用户的基本信息
void showCurrentUserData();
// 控制主菜单页面程序
bool isMainMenuRunning = false;

// 接收线程
void readTaskHandler(int clientfd);
// 获取系统时间(聊天信息需要添加时间信息)
string getCurrentTime();
// 主聊天页面程序
void mainMenu(int clientfd);

// 聊天客户端程序实现, main线程用作发送线程, 子线程用作接收线程
int main(int argc, char **argv)
{
    if (argc < 3)
    {
        cerr << "command invalid! example: ./client 127.0.0.1 8001" << endl;
        exit(-1);
    }

    // 解析通过命令行参数传递的ip和port
    char *ip = argv[1];
    int port = atoi(argv[2]);

    // 创建client端的socket
    int clientfd = socket(AF_INET, SOCK_STREAM, 0);
    if (-1 == clientfd)
    {
        cerr << "socket create failed!" << endl;
        exit(-1);
    }

    // 填写client需要连接的server信息ip+port
    sockaddr_in serverAddr;
    memset(&serverAddr, 0, sizeof(sockaddr_in));

    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(port);
    serverAddr.sin_addr.s_addr = inet_addr(ip);

    // client和server建立连接
    if (-1 == connect(clientfd, (sockaddr *)&serverAddr, sizeof(sockaddr_in)))
    {
        cerr << "connect server failed!" << endl;
        close(clientfd);
        exit(-1);
    }

    for (;;)
    {
        // 显示首页面菜单 登录、注册、退出
        cout << "=====================" << endl;
        cout << "1. 登录" << endl;
        cout << "2. 注册" << endl;
        cout << "3. 退出" << endl;
        cout << "=====================" << endl;
        cout << "请选择: ";
        int choice;
        cin >> choice;
        cin.get(); // 读掉缓冲区残留的回车

        switch (choice)
        {
        case 1: // login业务
        {
            int id = 0;
            char password[50] = {0};
            cout << "请输入你的id: ";
            cin >> id;
            cin.get(); // 读掉缓冲区残留的回车
            cout << "请输入你的密码: ";
            cin.getline(password, 50);

            json js;
            js["msgid"] = LOGIN_MSG;
            js["id"] = id;
            js["password"] = password;
            string request = js.dump();

            int len = send(clientfd, request.c_str(), strlen(request.c_str()) + 1, 0);
            if (len == -1)
            {
                cerr << "登陆消息发送失败!" << endl;
            }
            else
            {
                char buff[1024] = {0};
                len = recv(clientfd, buff, sizeof(buff), 0);
                if (len == -1)
                {
                    cerr << "接收消息失败!" << endl;
                }
                else
                {
                    json responsejs = json::parse(buff);
                    if (responsejs["errno"].get<int>() != 0) // 登录失败
                    {
                        cerr << responsejs["errmsg"] << endl;
                    }
                    else // 登录成功
                    {
                        // 记录当前用户的id和name
                        g_currentUser.setId(responsejs["id"].get<int>());
                        g_currentUser.setName(responsejs["name"]);

                        // 记录当前用户的好友列表消息
                        if (responsejs.contains("friends"))
                        {
                            // 初始化好友列表信息
                            g_currentUserFriendList.clear();

                            vector<string> friends = responsejs["friends"];
                            for (auto &friendName : friends)
                            {
                                json js = json::parse(friendName);
                                User user;
                                user.setId(js["id"].get<int>());
                                user.setName(js["name"]);
                                user.setState(js["state"]);
                                g_currentUserFriendList.push_back(user);
                            }
                        }

                        // 记录当前用户的群组列表消息
                        if (responsejs.contains("groups"))
                        {
                            // 初始化
                            g_currentUserGroupList.clear();

                            vector<string> groups = responsejs["groups"];
                            for (auto &groupName : groups)
                            {
                                json js = json::parse(groupName);
                                Group group;
                                group.setId(js["id"].get<int>());
                                group.setName(js["groupname"]);
                                group.setDesc(js["groupdesc"]);

                                vector<string> members = js["users"];
                                for (auto &memberName : members)
                                {
                                    GroupUser groupUser;
                                    json js = json::parse(memberName);
                                    groupUser.setId(js["id"].get<int>());
                                    groupUser.setName(js["name"]);
                                    groupUser.setState(js["state"]);
                                    groupUser.setRole(js["role"]);
                                    group.getUsers().push_back(groupUser);
                                }

                                g_currentUserGroupList.push_back(group);
                            }
                        }
                        showCurrentUserData();

                        // 显示当前用户的离线消息 个人聊天信息 群组聊天信息
                        if (responsejs.contains("offlinemsg"))
                        {
                            vector<string> offlinemsg = responsejs["offlinemsg"];
                            for (auto &msg : offlinemsg)
                            {
                                json js = json::parse(msg);
                                int msgType = js["type"].get<int>();
                                if (ONE_CHAT_MSG == msgType)
                                {
                                    cout << js["time"] << "[" << js["id"] << "]" << js["name"]
                                         << "said: " << js["msg"] << endl;
                                }
                                else
                                {
                                    cout << "群消息[" << js["groupid"] << "]" << js["time"] << "[" << js["id"]
                                         << "]" << js["name"] << "said: " << js["msg"] << endl;
                                }
                            }
                        }

                        // 登录成功, 启动接收线程负责接收数据, 该线程只启动一次
                        static int readthreadnumber = 0;
                        if (readthreadnumber == 0)
                        {
                            std::thread readTask(readTaskHandler, clientfd); // pthread_create
                            readTask.detach();                               // pthread_detach
                            readthreadnumber++;
                        }

                        // 进入聊天主菜单界面
                        isMainMenuRunning = true;
                        mainMenu(clientfd);
                    }
                }
            }
        }
        case 2: // register业务
        {
            char name[50] = {0};
            char password[50] = {0};
            cout << "请输入你的昵称: ";
            cin.getline(name, 50);
            cout << "请输入你的密码: ";
            cin.getline(password, 50);

            json js;
            js["msgid"] = REG_MSG;
            js["name"] = name;
            js["password"] = password;
            string request = js.dump();

            int len = send(clientfd, request.c_str(), strlen(request.c_str()) + 1, 0);
            if (len == -1)
            {
                cerr << "注册消息发送失败!" << endl;
            }
            else
            {
                char buff[1024] = {0};
                len = recv(clientfd, buff, sizeof(buff), 0);
                if (len == -1)
                {
                    cerr << "接收消息失败!" << endl;
                }
                else
                {
                    json responsejs = json::parse(buff);
                    if (responsejs["errno"].get<int>() != 0)
                    {
                        cerr << name << "该用户名已被注册过了! 请使用其他用户名注册!" << endl;
                    }
                    else
                    {
                        cout << name << "注册成功, 你的id为" << responsejs["id"]
                             << ", 你的密码是" << password << ", 请牢记你的密码!" << endl;
                    }
                }
            }
        }
        case 3: // quit业务
            close(clientfd);
            exit(0);
        default:
            cerr << "invalid input!" << endl;
            break;
        }
    }

    return 0;
}

// 接收线程
void readTaskHandler(int clientfd)
{
    for (;;)
    {
        char buff[1024] = {0};
        int len = recv(clientfd, buff, sizeof(buff), 0); // 阻塞了
        if (len == -1 || len == 0)
        {
            close(clientfd);
            exit(-1);
        }

        // 接收ChatServer转发的数据, 反序列化生成json数据对象
        json js = json::parse(buff);
        if (ONE_CHAT_MSG == js["msgid"].get<int>())
        {
            cout << js["time"] << "[" << js["id"] << "]" << js["name"]
                 << "said: " << js["msg"] << endl;
            continue;
        }
        else if (GROUP_CHAT_MSG == js["msgid"].get<int>())
        {
            cout << "群消息[" << js["groupid"] << "]" << js["time"] << "[" << js["id"] << "]" << js["name"]
                 << "said: " << js["msg"] << endl;
        }
    }
}

// "help" command handler
void help(int = 0, string str = "");
// "chat" command handler
void chat(int, string);
// "addfriend" command handler
void addFriend(int, string);
// "delfriend" command handler
void delFriend(int, string);
// "creategroup" command handler
void createGroup(int, string);
// "joingroup" command handler
void joinGroup(int, string);
// "quitgroup" command handler
void quitGroup(int, string);
// "groupchat" command handler
void groupChat(int, string);
// "loginout" command handler
void loginOut(int, string);

// 系统支持的客户端命令列表
unordered_map<string, string> commandMap = {
    {"help", "显示所有支持的命令, 格式为: help"},
    {"chat", "一对一聊天, 格式为: chat:friendid:msg"},
    {"addfriend", "添加好友, 格式为: addfriend:friendid"},
    {"delfriend", "删除好友, 格式为: delfriend:friendid"},
    {"creategroup", "创建群组, 格式为: creategroup:groupname:groupdesc"},
    {"joingroup", "加入群组, 格式为: joingroup:groupid"},
    {"quitgroup", "退出群组, 格式为: quitgroup:groupid"},
    {"groupchat", "群组聊天, 格式为: groupchat:groupid:msg"},
    {"loginout", "退出登录, 格式为: loginout"},
};

// 注册系统支持的客户端命令处理
unordered_map<string, function<void(int, string)>> commandHandlerMap = {
    {"help", help},
    {"chat", chat},
    {"addfriend", addFriend},
    {"delfriend", delFriend},
    {"creategroup", createGroup},
    {"joingroup", joinGroup},
    {"quitgroup", quitGroup},
    {"groupchat", groupChat},
    {"loginout", loginOut},
};

// 主聊天页面程序
void mainMenu(int clientfd)
{
    help();

    char buffer[1024] = {0};
    while (isMainMenuRunning)
    {
        cin.getline(buffer, sizeof(buffer));
        string cmdbuf(buffer);
        string cmd; // 存储命令
        int idx = cmdbuf.find(":");
        if (idx == -1)
        {
            cmd = cmdbuf;
        }
        else
        {
            cmd = cmdbuf.substr(0, idx);
        }
        auto it = commandHandlerMap.find(cmd);
        if (it == commandHandlerMap.end())
        {
            cerr << "不支持的命令, 请输入help查看支持的命令列表!" << endl;
            continue;
        }
        // 调用相应命令的事件处理回调, mainMenu对修改封闭, 添加新功能时, 只需要添加命令处理回调即可
        it->second(clientfd, cmdbuf.substr(idx + 1, cmdbuf.size() - idx)); // 调用命令处理函数
    }
}

void showCurrentUserData()
{
    cout << "================================login==============================" << endl;
    cout << "当前用户id: " << g_currentUser.getId() << endl;
    cout << "当前用户昵称: " << g_currentUser.getName() << endl;
    cout << "当前用户状态: " << g_currentUser.getState() << endl;
    cout << "==============================friend list=============================" << endl;
    cout << "当前用户好友列表: " << endl;

    if (!g_currentUserFriendList.empty())
    {
        for (auto &user : g_currentUserFriendList)
        {
            cout << "用户id: " << user.getId() << ", 昵称: " << user.getName() << ", 状态: " << user.getState() << endl;
            cout << "用户状态: " << user.getState() << endl;
        }
    }
    cout << "==============================group list=============================" << endl;
    cout << "当前用户群组列表: " << endl;
    if (!g_currentUserGroupList.empty())
    {
        for (auto &group : g_currentUserGroupList)
        {
            cout << "群组id: " << group.getId() << ", 名称: " << group.getName() << ", 描述: " << group.getDesc() << endl;
            for (auto &user : group.getUsers())
            {
                cout << "用户id: " << user.getId() << ", 昵称: " << user.getName() << ", 状态: " << user.getState() << endl;
                cout << "用户角色: " << user.getRole() << endl;
            }
        }
    }
}

string getCurrentTime()
{
    time_t t = time(0);
    char tmp[64];
    strftime(tmp, sizeof(tmp), "%Y-%m-%d %H:%M:%S", localtime(&t));
    return tmp;
}

// "help" 命令处理函数
void help(int, string)
{
    cout << "支持的命令列表: " << endl;
    for (auto &cmd : commandMap)
    {
        cout << cmd.first << ": " << cmd.second << endl;
    }
    cout << "请输入命令, 如: help" << endl;
}

// "chat" 命令处理函数
void chat(int clientfd, string cmdbuf)
{

    int idx = cmdbuf.find(":");
    if (idx == -1)
    {
        cerr << "命令格式错误, 请输入help查看支持的命令列表!" << endl;
        return;
    }
    int friendid = atoi(cmdbuf.substr(0, idx).c_str());
    string msg = cmdbuf.substr(idx + 1, cmdbuf.size() - idx);

    json js;
    js["msgid"] = ONE_CHAT_MSG;
    js["id"] = g_currentUser.getId();
    js["name"] = g_currentUser.getName();
    js["to"] = friendid;
    js["msg"] = msg;
    js["time"] = getCurrentTime();
    string buffer = js.dump();

    int len = send(clientfd, buffer.c_str(), strlen(buffer.c_str()) + 1, 0);
    if (len == -1)
    {
        cerr << "发送聊天消息失败! -> " << cmdbuf << endl;
    }
}

// "addfriend" 命令处理函数
void addFriend(int clientfd, string cmdbuf)
{
    int friendid = atoi(cmdbuf.c_str());
    json js;
    js["msgid"] = ADD_FRIEND_MSG;
    js["id"] = g_currentUser.getId();
    js["friendid"] = friendid;
    string buffer = js.dump();

    int len = send(clientfd, buffer.c_str(), buffer.size(), 0);
    if (len == -1)
    {
        cerr << "发送添加好友请求失败! -> " << cmdbuf << endl;
    }
}

// "delfriend" 命令处理函数
void delFriend(int clientfd, string cmdbuf)
{
    int friendid = atoi(cmdbuf.c_str());
    json js;
    js["msgid"] = DEL_FRIEND_MSG;
    js["id"] = g_currentUser.getId();
    js["friendid"] = friendid;
    string buffer = js.dump();

    int len = send(clientfd, buffer.c_str(), strlen(buffer.c_str()) + 1, 0);
    if (len == -1)
    {
        cerr << "发送删除好友请求失败! -> " << cmdbuf << endl;
    }
}

// "creategroup" 命令处理函数
void createGroup(int clientfd, string cmdbuf)
{
    int idx = cmdbuf.find(":");
    if (idx == -1)
    {
        cerr << "命令格式错误, 请输入help查看支持的命令列表!" << endl;
        return;
    }
    string groupname = cmdbuf.substr(0, idx);
    string groupdesc = cmdbuf.substr(idx + 1, cmdbuf.size() - idx);

    json js;
    js["msgid"] = CREATE_GROUP_MSG;
    js["id"] = g_currentUser.getId();
    js["name"] = groupname;
    js["desc"] = groupdesc;
    string buffer = js.dump();
    int len = send(clientfd, buffer.c_str(), strlen(buffer.c_str()) + 1, 0);
    if (len == -1)
    {
        cerr << "发送创建群组请求失败! -> " << cmdbuf << endl;
    }
}

// "joingroup" 命令处理函数
void joinGroup(int clientfd, string cmdbuf)
{
    int groupid = atoi(cmdbuf.c_str());
    json js;
    js["msgid"] = ADD_GROUP_MSG;
    js["id"] = g_currentUser.getId();
    js["groupid"] = groupid;
    string buffer = js.dump();

    int len = send(clientfd, buffer.c_str(), strlen(buffer.c_str()) + 1, 0);
    if (len == -1)
    {
        cerr << "发送加入群组请求失败! -> " << cmdbuf << endl;
    }
}

// "quitgroup" 命令处理函数
void quitGroup(int clientfd, string cmdbuf)
{
    int groupid = atoi(cmdbuf.c_str());
    json js;
    js["msgid"] = QUIT_GROUP_MSG;
    js["id"] = g_currentUser.getId();
    js["groupid"] = groupid;
    string buffer = js.dump();

    int len = send(clientfd, buffer.c_str(), strlen(buffer.c_str()) + 1, 0);
    if (len == -1)
    {
        cerr << "发送退出群组请求失败! -> " << cmdbuf << endl;
    }
}

// "groupchat" 命令处理函数
void groupChat(int clientfd, string cmdbuf)
{
    int idx = cmdbuf.find(":");
    if (idx == -1)
    {
        cerr << "命令格式错误, 请输入help查看支持的命令列表!" << endl;
        return;
    }
    int groupid = atoi(cmdbuf.substr(0, idx).c_str());
    string msg = cmdbuf.substr(idx + 1, cmdbuf.size() - idx);
    json js;
    js["msgid"] = GROUP_CHAT_MSG;
    js["id"] = g_currentUser.getId();
    js["name"] = g_currentUser.getName();
    js["groupid"] = groupid;
    js["msg"] = msg;
    js["time"] = getCurrentTime();
    string buffer = js.dump();

    int len = send(clientfd, buffer.c_str(), strlen(buffer.c_str()) + 1, 0);
    if (len == -1)
    {
        cerr << "发送群聊消息失败! -> " << cmdbuf << endl;
    }
}

// "loginout" 命令处理函数
void loginOut(int clientfd, string cmdbuf)
{
    json js;
    js["msgid"] = LOGIN_OUT_MSG;
    js["id"] = g_currentUser.getId();
    string buffer = js.dump();
    int len = send(clientfd, buffer.c_str(), strlen(buffer.c_str()) + 1, 0);
    if (len == -1)
    {
        cerr << "发送登出请求失败! -> " << cmdbuf << endl;
    }
    else
    {
        isMainMenuRunning = false;
    }
}
