#ifndef REDIS_H
#define REDIS_H

#include <hiredis/hiredis.h>
#include <thread>
#include <functional>
using namespace std;

class Redis
{
public:
    Redis(/* args */);
    ~Redis();

    // 连接redis服务器
    bool connet();

    // 向redis指定的通道channel发布消息
    bool publish(int channel, string message);

    // 向redis指定的通道subsrcibe订阅消息
    bool subsrcibe(int channel);

     // 向redis指定的通道取消subsrcibe订阅消息
    bool unsubsrcibe(int channel);

    // 在独立线程中接收订阅通道中的消息
    void observer_channel_message();

    // 初始化业务层上报通道消息的回调对象
    void init_notify_handler(function<void(int, string)> fn);

private:
    // hirdise同步上下文对象，负责publish消息
    redisContext *_pusblish_context;

    // hiredis同步上下文对象，负责subcribe消息
    redisContext *_subcribe_context;

    // 回调操作，收到订阅的消息，给service层上报
    function<void(int, string)> _notify_message_handler;
};

#endif