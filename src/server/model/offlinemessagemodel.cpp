#include "offlinemessagemodel.hpp"
#include "db.hpp"
#include "mysqlpool.hpp"
#include <iostream>
using namespace std;

// 存储用户的离线消息
void OfflineMsgModel::insert(int userid, string msg)
{
    // 1. 组装SQL语句
    char sql[1024] = {0};
    sprintf(sql, "insert into OfflineMessage VALUES(%d, '%s')",
            userid, msg.c_str());

    MYSQL *conn = MySQLPool::instance().getConnection();
    if (conn)
    {
        if (mysql_query(conn, sql) != 0)
        {
            cout << "insert offline message error: " << mysql_error(conn) << endl;
        }
        MySQLPool::instance().releaseConnection(conn);
    }
}
// 删除用户的离线消息
void OfflineMsgModel::remove(int userid)
{
    // 1. 组装SQL语句
    char sql[1024] = {0};
    sprintf(sql, "delete from OfflineMessage where userid = %d",
            userid);

    MYSQL *conn = MySQLPool::instance().getConnection();
    if (conn)
    {
        if (mysql_query(conn, sql) != 0)
        {
            cout << "remove offline message error: " << mysql_error(conn) << endl;
        }
        MySQLPool::instance().releaseConnection(conn);
    }
}
// 获取用户的离线消息
vector<string> OfflineMsgModel::query(int userid)
{
    // 1. 组装SQL语句
    char sql[1024] = {0};
    sprintf(sql, "select message from OfflineMessage where userid = %d",
            userid);

    vector<string> vec;
    MYSQL *conn = MySQLPool::instance().getConnection();
    if (conn)
    {
        if (mysql_query(conn, sql) == 0)
        {
            MYSQL_RES *res = mysql_use_result(conn);
            if (res != nullptr)
            {
                // 把userid用户的所有离线消息放入vec中返回
                MYSQL_ROW row;
                while ((row = mysql_fetch_row(res)) != nullptr)
                {
                    vec.push_back(row[0]);
                }
                mysql_free_result(res);
            }
        }
        else
        {
            cout << "query offline message error: " << mysql_error(conn) << endl;
        }
        MySQLPool::instance().releaseConnection(conn);
    }
    return vec;
}