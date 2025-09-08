#include "redis.hpp"

#include <string>
#include <iostream>

Redis::Redis()
    : publish_context_(nullptr),
      subscribe_context_(nullptr)
{
}
Redis::~Redis()
{
    if (publish_context_ != nullptr)
    {
        redisFree(publish_context_);
    }
    if (subscribe_context_ != nullptr)
    {
        redisFree(subscribe_context_);
    }
}

// 连接redis服务器
bool Redis::connect()
{
    // 负责publish发布消息的上下文连接
    publish_context_ = redisConnect("127.0.0.1", 6379);
    if (publish_context_ == nullptr)
    {
        cerr << "connect to redis server failed! " << endl;
        return false;
    }

    // 负责subscribe订阅消息的上下文连接
    subscribe_context_ = redisConnect("127.0.0.1", 6379);
    if (subscribe_context_ == nullptr)
    {
        cerr << "connect to redis server failed! " << endl;
        return false;
    }

    // 在单独的线程中, 监听通道上的事件, 有消息给业务层上报
    thread t([&]()
             { observer_channel_message(); });
    t.detach();

    cout << "connect to redis server success!" << endl;

    return true;
}

// 向redis指定的通道channel发布消息
bool Redis::publish(int channel, string message)
{
    redisReply *reply = (redisReply *)redisCommand(publish_context_, "PUBLISH %d %s", channel, message.c_str());
    if (reply == nullptr)
    {
        cerr << "publish message failed! " << endl;
        return false;
    }
    freeReplyObject(reply);
    return true;
}

// 向redis指定的通道channel订阅消息
bool Redis::subscribe(int channel)
{
    // SUBSCRIBE命令本身会造成线程阻塞等待通道里面发生消息. 这里只做订阅通道, 不接收通道消息
    // 通道消息的接收专门在observer_channel_message函数中的独立线程中进行
    // 只负责发送命令, 不阻塞接收redis server响应消息, 否则和notifyMsg线程抢占响应资源
    if (redisAppendCommand(subscribe_context_, "SUBSCRIBE %d", channel) == REDIS_ERR)
    {
        cerr << "subscribe channel failed! " << endl;
        return false;
    }
    // redisBufferWrite可以循环发送缓冲区
    int done = 0;
    while (!done)
    {
        if (redisBufferWrite(subscribe_context_, &done) == REDIS_ERR)
        {
            cerr << "subscribe channel failed! " << endl;
            return false;
        }
    }
    return true;
}

// 向redis指定的通道unsubscribe取消订阅消息
void Redis::unsubscribe(int channel) {}

// 在独立线程中接收订阅通道中的消息
void Redis::observer_channel_message()
{
    redisReply *reply = nullptr;
    while (redisGetReply(subscribe_context_, (void **)&reply) == REDIS_OK)
    {
        // 订阅收到的消息是一个带三元素的数组
        if (reply != nullptr && reply->element[2] != nullptr && reply->element[2]->str != nullptr)
        {
            // 给业务层上报通道上发生的消息
            notify_message_handler_(atoi(reply->element[1]->str), reply->element[2]->str);
        }

        freeReplyObject(reply);
    }

    cerr << ">>>>>>>>>>>>>>>> observer channel message failed! <<<<<<<<<<<<<<<" << endl;
}

// 初始化向业务层上报通道消息的回调对象
void Redis::init_notify_handler(function<void(int, string)> fn)
{
    notify_message_handler_ = fn;
}