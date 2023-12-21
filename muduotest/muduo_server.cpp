#include<iostream>
#include<muduo/net/TcpServer.h>
#include<muduo/net/EventLoop.h>
#include<string>
using namespace std;
using namespace muduo;
using namespace muduo::net;
using namespace placeholders;
/*基于muduo网络库开发服务器程序
1.组合TcpServer对象
2.创建EventLoop事件循环对象的指针
3.明确TcpServer构造函数需要什么参数，输出ChatServer的构造函数
4.在当前服务器类的构造函数当中，注册处理链接的回调函数和处理读写事件的回调函数
5.设置合适的服务器端线程数量，muduo库会自己分配I/O线程和worker线程
*/
class ChatServer
{
public:
    ChatServer(EventLoop *loop,
                const InetAddress &listenAddr,
                const string nameArg)
                :_server(loop,listenAddr,nameArg),_loop(loop)
    {
        //给服务器注册用户连接断开的回调
        _server.setConnectionCallback(bind(&ChatServer::onConnection,this,_1));
        //给服务器注册用户读写事件回调
        _server.setMessageCallback(bind(&ChatServer::onMessage,this,_1,_2,_3));
        //设置服务器端的线程数量
        _server.setThreadNum(4);
    }
    void start()//事件循环
    {
        _server.start();
    }
private:
    TcpServer _server;
    EventLoop *_loop;

    void onConnection(const TcpConnectionPtr &conn)
    {
        if(conn->connected())//连接
        {
            cout<<conn->peerAddress().toIpPort()<<" -> "
            <<conn->localAddress().toIpPort()<<endl;
        }
        else//断开连接
        {
            cout<<conn->peerAddress().toIpPort()<<" -> "
            <<conn->localAddress().toIpPort()<<endl;
            conn->shutdown();
        }
    }
    //用户读写事件
    void onMessage(const TcpConnectionPtr &conn,
                    Buffer *buffer,
                    Timestamp time)
    {
        string buf=buffer->retrieveAllAsString();
        cout<<"recv data:"<<buf <<"time:" <<time.toString()<<endl;
        conn->send(buf);
    }

};

int main()
{
    EventLoop loop;
    InetAddress addr("127.0.0.1",6000);
    ChatServer server(&loop,addr,"ChatServer");

    server.start();
    loop.loop();//epoll_wait以阻塞方式等待新用户连接，以连接用户的读写事件
}