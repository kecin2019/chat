#include "groupmodel.hpp"
#include "db.hpp"
#include "mysqlpool.hpp"
#include <iostream>
using namespace std;

// 创建群组
bool GroupModel::createGroup(Group &group)
{
    char sql[1024] = {0};
    sprintf(sql, "insert into Allgroup(groupname, groupdesc) VALUES('%s', '%s')",
            group.getName().c_str(), group.getDesc().c_str());

    MYSQL *conn = MySQLPool::instance().getConnection();
    if (conn)
    {
        if (mysql_query(conn, sql) == 0)
        {
            group.setId(mysql_insert_id(conn));
            MySQLPool::instance().releaseConnection(conn);
            return true;
        }
        else
        {
            cout << "createGroup error: " << mysql_error(conn) << endl;
        }
        MySQLPool::instance().releaseConnection(conn);
    }
    return false;
}

// 加入群组
bool GroupModel::joinGroup(int userid, int groupid, string role)
{
    char sql[1024] = {0};
    sprintf(sql, "insert into GroupUser VALUES(%d, %d, '%s')",
            groupid, userid, role.c_str());

    MYSQL *conn = MySQLPool::instance().getConnection();
    if (conn)
    {
        if (mysql_query(conn, sql) == 0)
        {
            MySQLPool::instance().releaseConnection(conn);
            return true;
        }
        else
        {
            cout << "joinGroup error: " << mysql_error(conn) << endl;
        }
        MySQLPool::instance().releaseConnection(conn);
    }
    return false;
}

// 退出群组
bool GroupModel::quitGroup(int userid, int groupid)
{
    char sql[1024] = {0};
    sprintf(sql, "delete from GroupUser where userid = %d and groupid = %d",
            userid, groupid);

    MYSQL *conn = MySQLPool::instance().getConnection();
    if (conn)
    {
        if (mysql_query(conn, sql) == 0)
        {
            MySQLPool::instance().releaseConnection(conn);
            return true;
        }
        else
        {
            cout << "quitGroup error: " << mysql_error(conn) << endl;
        }
        MySQLPool::instance().releaseConnection(conn);
    }
    return false;
}

// 查询用户所在群组信息
vector<Group> GroupModel::queryGroups(int userId)
{
    /*
     * 1. 先根据userid在GroupUser表中查询出该用户所属的群组信息
     * 2. 再根据群组信息, 查询属于该群组的所有用户的userid, 并且和user表进行多表联合查询, 查出用户的详细信息
     */
    char sql[1024] = {0};
    sprintf(sql, "select a.id, a.groupname, a.groupdesc from Allgroup a join GroupUser as b on a.id = b.groupid where b.userid = %d",
            userId);

    vector<Group> groupVec;
    MYSQL *conn = MySQLPool::instance().getConnection();
    if (conn)
    {
        if (mysql_query(conn, sql) == 0)
        {
            MYSQL_RES *res = mysql_use_result(conn);
            if (res != nullptr)
            {
                MYSQL_ROW row;
                while ((row = mysql_fetch_row(res)) != nullptr)
                {
                    Group group;
                    group.setId(atoi(row[0]));
                    group.setName(row[1]);
                    group.setDesc(row[2]);
                    groupVec.push_back(group);
                }
                mysql_free_result(res);
            }
        }
        else
        {
            cout << "queryGroups error: " << mysql_error(conn) << endl;
        }

        // 查询群组的用户信息
        for (auto &group : groupVec)
        {
            sprintf(sql, "select a.id, a.name from User a \
                    inner join GroupUser as b on b.userid = a.id where b.groupid = %d and b.userid != %d",
                    group.getId(), userId);
            if (mysql_query(conn, sql) == 0)
            {
                MYSQL_RES *res = mysql_use_result(conn);
                if (res != nullptr)
                {
                    MYSQL_ROW row;
                    while ((row = mysql_fetch_row(res)) != nullptr)
                    {
                        GroupUser user;
                        user.setId(atoi(row[0]));
                        user.setName(row[1]);
                        user.setState(row[2]);
                        user.setRole(row[3]);
                        group.getUsers().push_back(user);
                    }
                    mysql_free_result(res);
                }
            }
        }
        MySQLPool::instance().releaseConnection(conn);
    }
    return groupVec;
}

// 根据指定的groupId查询群组用户id列表, 除userid自己, 主要用户群聊业务给其它群组其它成员群发消息
vector<int> GroupModel::queryGroupUsers(int userid, int groupid)
{
    char sql[1024] = {0};
    sprintf(sql, "select userid from GroupUser where groupid = %d and userid != %d",
            groupid, userid);

    vector<int> userVec;
    MYSQL *conn = MySQLPool::instance().getConnection();
    if (conn)
    {
        if (mysql_query(conn, sql) == 0)
        {
            MYSQL_RES *res = mysql_use_result(conn);
            if (res != nullptr)
            {
                MYSQL_ROW row;
                while ((row = mysql_fetch_row(res)) != nullptr)
                {
                    userVec.push_back(atoi(row[0]));
                }
                mysql_free_result(res);
            }
        }
        else
        {
            cout << "queryGroupUsers error: " << mysql_error(conn) << endl;
        }
        MySQLPool::instance().releaseConnection(conn);
    }
    return userVec;
}
