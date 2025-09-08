#include "mysqlpool.hpp"
#include <muduo/base/Logging.h>

MySQLPool &MySQLPool::instance()
{
    static MySQLPool pool;
    return pool;
}

bool MySQLPool::init(std::size_t poolSize,
                     const std::string &host,
                     const std::string &user,
                     const std::string &pass,
                     const std::string &db,
                     unsigned int port)
{
    std::lock_guard<std::mutex> lock(mtx_);
    if (isInit_)
        return true;
    maxSize_ = poolSize;
    host_ = host;
    user_ = user;
    pass_ = pass;
    db_ = db;
    port_ = port;
    for (std::size_t i = 0; i < poolSize; ++i)
    {
        MYSQL *conn = createConnection();
        if (!conn)
            return false;
        connections_.push(conn);
    }
    isInit_ = true;
    return true;
}

MYSQL *MySQLPool::createConnection()
{
    MYSQL *conn = mysql_init(nullptr);
    if (!conn)
        return nullptr;
    MYSQL *p = mysql_real_connect(conn, host_.c_str(), user_.c_str(),
                                  pass_.c_str(), db_.c_str(), port_, nullptr, 0);
    if (p)
    {
        mysql_query(conn, "set names utf8");
        return conn;
    }
    mysql_close(conn);
    return nullptr;
}

MYSQL *MySQLPool::getConnection()
{
    std::unique_lock<std::mutex> lock(mtx_);
    cond_.wait(lock, [this]
               { return !connections_.empty(); });
    MYSQL *conn = connections_.front();
    connections_.pop();
    return conn;
}

void MySQLPool::releaseConnection(MYSQL *conn)
{
    std::lock_guard<std::mutex> lock(mtx_);
    if (conn)
    {
        connections_.push(conn);
        cond_.notify_one();
    }
}

MySQLPool::~MySQLPool()
{
    std::lock_guard<std::mutex> lock(mtx_);
    while (!connections_.empty())
    {
        MYSQL *conn = connections_.front();
        connections_.pop();
        if (conn)
            mysql_close(conn);
    }
}
