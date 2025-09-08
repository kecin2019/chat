#include "usermodel.hpp"
#include "db.hpp"
#include "mysqlpool.hpp"

#include <iostream>
using namespace std;

// User表的增加方法
bool UserModel::insert(User &user)
{
    // 1. 组装SQL语句
    char sql[1024] = {0};
    sprintf(sql, "insert into User(name, password, state) values('%s', '%s', '%s')",
            user.getName().c_str(), user.getPassword().c_str(), user.getState().c_str());

    MYSQL *conn = MySQLPool::instance().getConnection();
    if (conn)
    {
        if (mysql_query(conn, sql) == 0)
        {
            // 获取插入成功的用户数据生成的主键id
            user.setId(mysql_insert_id(conn));
            MySQLPool::instance().releaseConnection(conn);
            return true;
        }
        else
        {
            cout << "insert error: " << mysql_error(conn) << endl;
        }
        MySQLPool::instance().releaseConnection(conn);
    }
    return false;
}

User UserModel::query(int id)
{
    char sql[1024] = {0};
    sprintf(sql, "select * from User where id = %d", id);

    MYSQL *conn = MySQLPool::instance().getConnection();
    if (conn)
    {
        if (mysql_query(conn, sql) == 0)
        {
            MYSQL_RES *res = mysql_use_result(conn);
            if (res)
            {
                MYSQL_ROW row = mysql_fetch_row(res);
                if (row)
                {
                    User user;
                    user.setId(atoi(row[0]));
                    user.setName(row[1]);
                    user.setPassword(row[2]);
                    user.setState(row[3]);
                    mysql_free_result(res);
                    MySQLPool::instance().releaseConnection(conn);
                    return user;
                }
                mysql_free_result(res);
            }
        }
        MySQLPool::instance().releaseConnection(conn);
    }
    return User();
}

// 更新用户的状态信息
bool UserModel::updateState(User user)
{
    // 1. 组装SQL语句
    char sql[1024] = {0};
    sprintf(sql, "update User set state = '%s' where id = %d",
            user.getState().c_str(), user.getId());

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
            cout << "update error: " << mysql_error(conn) << endl;
        }
        MySQLPool::instance().releaseConnection(conn);
    }
    return false;
}

// 重置用户的状态信息
bool UserModel::resetState()
{
    char sql[1024] = {0};
    sprintf(sql, "update User set state = 'offline' where state = 'online'");

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
            cout << "update error: " << mysql_error(conn) << endl;
            MySQLPool::instance().releaseConnection(conn);
        }
    }
    return false;
}