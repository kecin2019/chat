#include "friendmodel.hpp"
#include "db.hpp"

// 添加好友方法
bool FriendModel::addFriend(int userid, int friendId)
{
    char sql[1024] = {0};
    sprintf(sql, "insert into Friend VALUES(%d, %d)",
            userid, friendId);
    MySQL mysql;
    if (mysql.connect())
    {
        mysql.update(sql);
        return true;
    }
    return false;
}

// 删除好友方法
bool FriendModel::delFriend(int userid, int friendId)
{
    char sql[1024] = {0};
    sprintf(sql, "delete from Friend where userid = %d and friendId = %d",
            userid, friendId);
    MySQL mysql;
    if (mysql.connect())
    {
        mysql.update(sql);
        return true;
    }
    return false;
}

// 查询好友列表方法
vector<User> FriendModel::queryFriends(int userid)
{
    // 1. 组装SQL语句
    char sql[1024] = {0};
    sprintf(sql, "SELECT a.id,a.name,a.state FROM User a INNER JOIN Friend b ON (b.friendid = a.id AND b.userid = %d)"
                 " OR (b.userid = a.id AND b.friendid = %d)",
            userid, userid);

    MySQL mysql;
    vector<User> vec;
    if (mysql.connect())
    {
        MYSQL_RES *res = mysql.query(sql);
        if (res != nullptr)
        {
            MYSQL_ROW row;
            while ((row = mysql_fetch_row(res)) != nullptr)
            {
                User user;
                user.setId(atoi(row[0]));
                user.setName(row[1]);
                user.setState(row[2]);
                vec.push_back(user);
            }
            mysql_free_result(res);
        }
    }
    return vec;
}