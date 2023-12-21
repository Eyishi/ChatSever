#include "redis.hpp"
#include <iostream>

Redis::Redis(/* args */)
    : _pusblish_context(nullptr), _subcribe_context(nullptr)
{
}

Redis::~Redis()
{
    if (_pusblish_context != nullptr)
    {
        redisFree(_pusblish_context);
    }
    if (_subcribe_context != nullptr)
    {
        redisFree(_subcribe_context);
    }
}

// 连接redis服务器
bool Redis::connet()
{
    // 负责publish发布消息的上下文连接
    _pusblish_context = redisConnect("127.0.0.1", 6379);
    if (nullptr == _pusblish_context)
    {
        cerr << "connect redis faild" << endl;
        return false;
    }

    // 负责subscribe订阅消息的上下文连接
    _subcribe_context = redisConnect("127.0.0.1", 6379);
    if (nullptr == _subcribe_context)
    {
        cerr << "connect redis faild" << endl;
        return false;
    }
    // 在单独的线程中，监听通道上的事件，有消息给业务层上报
    thread t([&]()
             { observer_channel_message(); });
    t.detach();
    cout << "connect redis-server success!" << endl;
    return true;
}

// 向redis指定的通道channel发布消息
bool Redis::publish(int channel, string message)
{
    redisReply *reply = static_cast<redisReply *>(redisCommand(_pusblish_context, "PUBLISH %d %s",
                                                               channel, message.c_str()));
    if (nullptr == reply)
    {
        cerr << "publish command failed!" << endl;
        return false;
    }
    freeReplyObject(reply);
    return true;
}

// 向redis指定的通道subsrcibe订阅消息
bool Redis::subsrcibe(int channel)
{
    // subsrcibe命令本身会造成线程阻塞等待通道里面发生消息，这里只做订阅通道，不接收通道消息
    //  通道消息的接收专门在observer_channel_message函数中的独立线程中进行
    // 只负责发送命令，不阻塞接收redis server响应消息，否则和notifyMsg线程抢占响应资源

    if (REDIS_ERR == redisAppendCommand(this->_subcribe_context, "SUBSCRIBE %d", channel))
    {
        cerr << "subsrcibe command failed!" << endl;
        return false;
    }
    // redisBufferWrite可以循环发送缓冲区，直到缓冲区数据发送完毕(done被置为1)
    int done = 0;
    while (!done)
    {
        if (REDIS_ERR == redisBufferWrite(this->_subcribe_context, &done))
        {
            cerr << "subsrcibe command failed!" << endl;
            return false;
        }
    }
    return true;
}
// 向redis指定的通道取消subsrcibe订阅消息
bool Redis::unsubsrcibe(int channel)
{
     if (REDIS_ERR == redisAppendCommand(this->_subcribe_context, "UNSUBSCRIBE %d", channel))
    {
        cerr << "unsubsrcibe command failed!" << endl;
        return false;
    }
    // redisBufferWrite可以循环发送缓冲区，直到缓冲区数据发送完毕(done被置为1)
    int done = 0;
    while (!done)
    {
        if (REDIS_ERR == redisBufferWrite(this->_subcribe_context, &done))
        {
            cerr << "unsubsrcibe command failed!" << endl;
            return false;
        }
    }
    return true;
}
// 在独立线程中接收订阅通道中的消息
void Redis::observer_channel_message()
{
    redisReply *reply = nullptr;
    while(REDIS_OK == redisGetReply(this->_subcribe_context,(void **)&reply))
    {
        //订阅收到的消息是一个带三元素的数据
        if(reply != nullptr && reply->element[2] !=nullptr && reply->element[2]->str !=nullptr)
        {
            //给业务层上报通道发送的消息 
            _notify_message_handler(atoi(reply->element[1]->str),reply->element[2]->str);
        }
        freeReplyObject(reply);
    }
    cerr<<">>>>>>>>>>>>>>>>observer_channel_message quit>>>>>>>>>>>>>>>>>>>>"<<endl;
}

// 初始化业务层上报通道消息的回调对象
void Redis::init_notify_handler(function<void(int, string)> fn)
{
    this->_notify_message_handler =fn;
}
