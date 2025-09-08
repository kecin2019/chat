#pragma once

#include <mysql/mysql.h>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <string>

// 连接池管理类，用于复用 MySQL 连接，避免频繁创建和销毁
class MySQLPool
{
public:
    // 获取单例对象
    static MySQLPool &instance();

    // 初始化连接池，提前创建指定数量的连接
    bool init(std::size_t poolSize,
              const std::string &host,
              const std::string &user,
              const std::string &pass,
              const std::string &db,
              unsigned int port = 3306);

    // 从连接池中获取一个可用的连接（阻塞直到有可用连接）
    MYSQL *getConnection();

    // 使用完连接后释放回池
    void releaseConnection(MYSQL *conn);

    // 禁止拷贝
    MySQLPool(const MySQLPool &) = delete;
    MySQLPool &operator=(const MySQLPool &) = delete;

    ~MySQLPool();

private:
    MySQLPool() = default;
    MYSQL *createConnection();

    std::queue<MYSQL *> connections_;
    std::mutex mtx_;
    std::condition_variable cond_;
    std::string host_;
    std::string user_;
    std::string pass_;
    std::string db_;
    unsigned int port_ = 3306;
    std::size_t maxSize_ = 0;
    bool isInit_ = false;
};
