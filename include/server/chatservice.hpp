#ifndef CHATSERVICE_H
#define CHATSERVICE_H

#include<muduo/net/TcpConnection.h>
#include<unordered_map>
#include<functional>
#include<mutex>

#include"json.hpp"
#include"usermodel.hpp"
#include"offlinemessagemodel.hpp"
#include"friendmodel.hpp"
#include"groupmodel.hpp"
#include"redis.hpp"
using namespace std;
using namespace muduo;
using namespace muduo::net;
using json=nlohmann::json;

//处理消息回调的方法类型
using MsgHandler =std::function<void(const TcpConnectionPtr &conn,json &js,Timestamp)>;
//聊天服务器业务类
class ChatService
{
public:
    //获取单例对象
    static ChatService *instance();
    //登录业务
    void login(const TcpConnectionPtr &conn,json &js,Timestamp time);
    //注册业务
    void reg(const TcpConnectionPtr &conn,json &js,Timestamp time);
    //获取消息对应的处理器
    MsgHandler getHandler(int msgin);
    //处理客户端异常退出
    void clientCloseException(const TcpConnectionPtr &conn);

    //服务器异常，业务重置方法
    void reset();

    //一对一聊天
    void oneChat(const TcpConnectionPtr &conn,json &js,Timestamp time);

    //添加好友
    void addFriend(const TcpConnectionPtr &conn,json &js,Timestamp time);

    //创建群组业务
    void createGroup(const TcpConnectionPtr &conn,json &js,Timestamp time);

    //加入群组业务
    void addGroup(const TcpConnectionPtr &conn,json &js,Timestamp time);

    //群聊业务
    void groupChat(const TcpConnectionPtr &conn,json &js,Timestamp time);

    //处理注销
    void loginout(const TcpConnectionPtr &conn,json &js,Timestamp time);

    void handlerRedisSubscribeMessage(int userid,string message);
private:
    ChatService();

    //存储消息id和其对应的处理方法
    unordered_map<int,MsgHandler> _msgHandlerMap;

    //存储在线用户的通信连接
    unordered_map<int,TcpConnectionPtr> _userConnMap;

    //定义互斥锁，保证_userConnMap线程安全
    mutex _connMutex;

    //数据库操作类对象
    UserModel _userModel;

    //离线数据操作对象
    OffMsgModel _offMsgModel;

    //操作好友类
    FriendModel _friendModel;

    //群聊操作
    GroupModel _groupModel;

    //redis操作对象
    Redis _redis;
};


#endif