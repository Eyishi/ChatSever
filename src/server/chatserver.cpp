#include"chatserver.hpp"
#include"chatservice.hpp"
#include"json.hpp"

#include<string>
#include<iostream>
#include<functional>
using namespace std;
using namespace placeholders;
using json=nlohmann::json;

ChatServer::ChatServer(EventLoop *loop,
                const InetAddress &listenAddr,
                const string nameArg)
                :_server(loop,listenAddr,nameArg),_loop(loop)
    {
            //注册连接回调
            
            _server.setConnectionCallback(bind(&ChatServer::onConnection,this,_1));
            //注册消息回调
            _server.setMessageCallback(bind(&ChatServer::onMessage,this,_1,_2,_3));
            //设置线程数量
            _server.setThreadNum(4);
            
    }
    //启动服务
    void ChatServer::start()
    {
        _server.start();
    }
    //连接相关信息回调函数
    void ChatServer::onConnection(const TcpConnectionPtr &conn)
    {
        //客户端断开连接
        if(!conn->connected())
        {
            ChatService::instance()->clientCloseException(conn);
            conn->shutdown();
        }
        
    }
    //上报读写事件相关回调
    void ChatServer::onMessage(const TcpConnectionPtr &conn,
                    Buffer *buffer,
                    Timestamp time)
    {
        string buf=buffer->retrieveAllAsString();
        //数据反序列化
        json js=json::parse(buf);
        //达到目的：完全解耦网络模块代码和业务模块代码
        //通过js["msgid"]获取=》业务处理hanlder =》conn js time
        ChatService *service= ChatService::instance();
        auto msgHandler= service->getHandler(js["msgid"].get<int>());
        //回调消息绑定好的处理器，来执行相应的业务处理
        msgHandler(conn,js,time);
    }