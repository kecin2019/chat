#pragma once

#include "user.hpp"

#include <vector>
using namespace std;

// 维护好友信息的操作接口方法
class FriendModel
{
public:
    // 添加好友方法
    bool addFriend(int userid, int friendId);
    // 删除好友方法
    bool delFriend(int userid, int friendId);
    // 查询好友列表方法
    vector<User> queryFriends(int userid);
};