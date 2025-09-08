#include "db.hpp"

#include <muduo/base/Logging.h>
// 数据库配置信息
static string server = "127.0.0.1";
static string user = "kecin";
static string password = "jkw123";
static string dbname = "chat";

// 初始化数据库连接
MySQL::MySQL()
{
    conn_ = mysql_init(nullptr);
}

// 释放数据库连接资源
MySQL::~MySQL()
{
    if (conn_ != nullptr)
    {
        mysql_close(conn_);
    }
}

// 连接数据库
bool MySQL::connect()
{
    MYSQL *p = mysql_real_connect(conn_, server.c_str(), user.c_str(),
                                  password.c_str(), dbname.c_str(), 3306, nullptr, 0);
    if (p != nullptr)
    {
        // C和C++代码默认的编码字符集是ASCII，如果不设置，从MySQL数据库读取中文时会显示？
        mysql_query(conn_, "set names utf8");
        LOG_INFO << " 数据库连接成功!";
    }
    else
    {
        LOG_INFO << " 数据库连接失败!";
    }
    return p;
}

// 更新操作
bool MySQL::update(string sql)
{
    if (mysql_query(conn_, sql.c_str()))
    {
        LOG_ERROR << sql << " 更新失败! 错误: " << mysql_error(conn_);
        return false;
    }
    else
    {
        LOG_INFO << sql << " 更新成功!";
    }
    return true;
}

// 查询操作
MYSQL_RES *MySQL::query(string sql)
{
    if (mysql_query(conn_, sql.c_str()))
    {
        LOG_INFO << __FILE__ << ":" << __LINE__ << ":"
                 << sql << "查询失败!";
        return nullptr;
    }
    return mysql_use_result(conn_);
}

// 获取连接
MYSQL *MySQL::getConn()
{
    return conn_;
}
