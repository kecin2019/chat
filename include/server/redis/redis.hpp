#pragma once

#include <hiredis/hiredis.h>
#include <thread>
#include <functional>
#include <string>
using namespace std;

class Redis
{
public:
    Redis();
    ~Redis();

    // 连接redis服务器
    bool connect();
    // 向redis指定的通道channel发布消息
    bool publish(int channel, string message);
    // 向redis指定的通道channel订阅消息
    bool subscribe(int channel);
    // 向redis指定的通道unsubscribe取消订阅消息
    void unsubscribe(int channel);
    // 在独立线程中接收订阅通道中的消息
    void observer_channel_message();
    // 初始化向业务层上报通道消息的回调对象
    void init_notify_handler(function<void(int, string)> fn);

private:
    // hiredis 同步上下文对象, 负责publish消息
    redisContext *publish_context_;
    // hiredis 同步上下文对象, 负责subscribe消息
    redisContext *subscribe_context_;

    // 回调操作, 收到订阅的消息, 给service层上报
    function<void(int, string)> notify_message_handler_;
};