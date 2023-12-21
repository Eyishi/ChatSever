#include "chatserver.hpp"
#include "chatservice.hpp"
#include "public.hpp"
#include "muduo/base/Logging.h"

#include <string>
#include <vector>
#include <map>
#include <iostream>
using namespace std;
using namespace muduo;

ChatService::ChatService()
{
    // 绑定对应的消息处理
    _msgHandlerMap.insert({LOGIN_MSG, std::bind(&ChatService::login, this, _1, _2, _3)});
    _msgHandlerMap.insert({REG_MSG, std::bind(&ChatService::reg, this, _1, _2, _3)});
    _msgHandlerMap.insert({ONE_CHAT_MSG, std::bind(&ChatService::oneChat, this, _1, _2, _3)});
    _msgHandlerMap.insert({ADD_FRIEND_MSG, std::bind(&ChatService::addFriend, this, _1, _2, _3)});
    _msgHandlerMap.insert({LOGINOUT_MSG, std::bind(&ChatService::loginout, this, _1, _2, _3)});

    // 群组业务相关回调事件处理回调注册
    _msgHandlerMap.insert({CREATE_GROUP_MSG, std::bind(&ChatService::createGroup, this, _1, _2, _3)});
    _msgHandlerMap.insert({ADD_GROUP_MSG, std::bind(&ChatService::addGroup, this, _1, _2, _3)});
    _msgHandlerMap.insert({GROUP_CHAT_MSG, std::bind(&ChatService::groupChat, this, _1, _2, _3)});

    // 连接redis服务器
    if (_redis.connet())
    {
        // 设置上报消息的回调
        _redis.init_notify_handler(std::bind(&ChatService::handlerRedisSubscribeMessage, this, _1, _2));
    }
}
// 获取单例对象
ChatService *ChatService::instance()
{
    static ChatService service;
    return &service;
}
MsgHandler ChatService::getHandler(int msgid)
{
    // 记录错误日志，msgid没有对应的事件处理回调
    auto it = _msgHandlerMap.find(msgid);
    if (it == _msgHandlerMap.end()) // 没有处理回调函数
    {
        // LOG_ERROR<<"msgid:"<<msgid<<"can not find handler";
        // 返回一个默认的处理器，空操作
        return [=](const TcpConnectionPtr &conn, json &js, Timestamp time)
        {
            LOG_ERROR << "msgid:" << msgid << "can not find handler";
        };
    }
    return _msgHandlerMap[msgid];
}

// 登录业务
void ChatService::login(const TcpConnectionPtr &conn, json &js, Timestamp time)
{
    int id = js["id"];
    string pwd = js["password"];

    User user = _userModel.query(id);
    if (user.getId() != -1 && user.getPassword() == pwd)
    {
        if (user.getState() == "online")
        {
            // 该用户已经登录，不允许重复登录
            json response;
            response["msgid"] = LOGIN_MSG_ACK;
            response["errno"] = 2;
            response["errmsg"] = "该账号已经登录，不允许重复登录";
            conn->send(response.dump());
        }
        else
        {
            // 登录成功，记录用户连接信息
            {
                lock_guard<mutex> lock(_connMutex);
                _userConnMap.insert({id, conn});
            }

            // id用户登录成功之后，向redis订阅channel(id)
            _redis.subsrcibe(id);

            // 登录成功，更新用户账号信息 offline->online
            user.setState("online");
            _userModel.updateState(user);

            json response;
            response["msgid"] = LOGIN_MSG_ACK;
            response["errno"] = 0;
            response["id"] = id;
            response["name"] = user.getName();

            // 查询该用户的好友信息并返回
            vector<User> userVec = _friendModel.query(id);
            if (!userVec.empty())
            {
                vector<string> vec2;
                for (auto &user : userVec)
                {
                    json js;
                    js["id"] = user.getId();
                    js["name"] = user.getName();
                    js["state"] = user.getState();
                    vec2.push_back(js.dump());
                }
                response["friends"] = vec2;
            }
            // 查询该用户的群组信息
            vector<Group> groupuserVec = _groupModel.queryGroups(id);
            if (!groupuserVec.empty())
            {
                vector<string> groupv;
                for (Group &group : groupuserVec)
                {
                    json grpjson;
                    grpjson["id"] = group.getId();
                    grpjson["groupname"] = group.getName();
                    grpjson["groupdesc"] = group.getDesc();

                    vector<string> userV;
                    for (GroupUser &user : group.getUsers())
                    {
                        json js;
                        js["id"] = user.getId();
                        js["name"] = user.getName();
                        js["state"] = user.getState();
                        js["role"] = user.getRole();
                        userV.push_back(js.dump());
                    }
                    grpjson["users"] = userV;
                    groupv.push_back(grpjson.dump());
                }
                response["groups"] = groupv;
            }
            // 查询该用户是否有离线消息
            vector<string> offlinevec = _offMsgModel.query(id);
            if (!offlinevec.empty())
            {
                response["offlinemsg"] = offlinevec;
                // 读取该用户的离线消息后，把该用户的所有离线消息删除掉
                _offMsgModel.remove(id);
            }

            conn->send(response.dump());
        }
    }
    else
    {
        // 该用户不存在，或者密码错误登录失败
        json response;
        response["msgid"] = LOGIN_MSG_ACK;
        response["errno"] = 1;
        response["errmsg"] = "用户名或密码错误";
        conn->send(response.dump());
    }
}
// 注册业务
void ChatService::reg(const TcpConnectionPtr &conn, json &js, Timestamp time)
{
    // 处理注册业务   name   password
    string name = js["name"];
    string pwd = js["password"];

    User user;
    user.setName(name);
    user.setPassword(pwd);
    bool state = _userModel.insert(user);
    if (state)
    {
        // 注册成功
        json response;
        response["msgid"] = REG_MSG_ACK;
        response["errno"] = 0;
        response["id"] = user.getId();
        conn->send(response.dump());
    }
    else
    {
        json response;
        response["msgid"] = REG_MSG_ACK;
        response["errno"] = 1;
        conn->send(response.dump());
    }
}
// 服务器异常，业务重置方法
void ChatService::reset()
{
    // 把所有online状态的用户，置为offline
    _userModel.resetState();
}
// 处理注销
void ChatService::loginout(const TcpConnectionPtr &conn, json &js, Timestamp time)
{
    int userid = js["id"].get<int>();
    {
        lock_guard<mutex> lock(_connMutex);
        auto it = _userConnMap.find(userid);
        if (it != _userConnMap.end())
        {
            _userConnMap.erase(it);
        }
    }
    // 用户注销，下线，在redis中取消订阅
    _redis.unsubsrcibe(userid);

    // 更新用户状态
    User user(userid, " ", " ", "offline");
    _userModel.updateState(user);
}
// 处理客户端异常退出
void ChatService::clientCloseException(const TcpConnectionPtr &conn)
{
    User user;
    {
        lock_guard<mutex> lock(_connMutex);

        for (auto it = _userConnMap.begin(); it != _userConnMap.end(); ++it)
        {
            if (it->second == conn)
            {
                user.setId(it->first);
                _userConnMap.erase(it);
                break;
            }
        }
    }
    // 用户注销，下线，在redis中取消订阅
    _redis.unsubsrcibe(user.getId());

    // 更新用户状态信息
    if (user.getId() != -1)
    {
        user.setState("offline");
        _userModel.updateState(user);
    }
}
// 一对一聊天
void ChatService::oneChat(const TcpConnectionPtr &conn, json &js, Timestamp time)
{
    int toid = js["toid"].get<int>(); // 转发到的用户

    {
        lock_guard<mutex> lock(_connMutex);
        // 直接从在线用户集合表里找，找到就转发，没有找到就存储离线消息
        auto it = _userConnMap.find(toid);
        if (it != _userConnMap.end())
        {
            // toid在线，服务器直接转发消息
            it->second->send(js.dump());
            return;
        }
    }
    // 有可能两用户不连接在同一个服务器上，要查看toid是否在线
    User user = _userModel.query(toid);
    if (user.getState() == "online")
    {
        _redis.publish(toid, js.dump());
        return;
    }
    // toid不在线，存储离线消息
    _offMsgModel.insert(toid, js.dump());
}
// 添加好友
void ChatService::addFriend(const TcpConnectionPtr &conn, json &js, Timestamp time)
{
    int userid = js["id"].get<int>();
    int frienid = js["friendid"].get<int>();

    // 存储好友信息
    _friendModel.insert(userid, frienid);
}

// 创建群组业务
void ChatService::createGroup(const TcpConnectionPtr &conn, json &js, Timestamp time)
{
    int userid = js["id"].get<int>();
    string name = js["groupname"];
    string desc = js["groupdesc"];

    // 存储新创建的群组信息
    Group group(-1, name, desc);
    if (_groupModel.createGroup(group))
    {
        // 存储群组创建人信息
        _groupModel.addGroup(userid, group.getId(), "creator");
    }
}

// 加入群组业务
void ChatService::addGroup(const TcpConnectionPtr &conn, json &js, Timestamp time)
{
    int userid = js["id"].get<int>();
    int groupid = js["groupid"].get<int>();
    _groupModel.addGroup(userid, groupid, "normal");
}
// 群聊业务
void ChatService::groupChat(const TcpConnectionPtr &conn, json &js, Timestamp time)
{
    int userid = js["id"].get<int>();
    int groupid = js["groupid"].get<int>();
    vector<int> useridVec = _groupModel.queryGroupUsers(userid, groupid);
    lock_guard<mutex> lock(_connMutex);
    for (int id : useridVec)
    {
        // 先在在线用户中发送
        auto it = _userConnMap.find(id);
        if (it != _userConnMap.end())
        {
            // 转发群消息
            it->second->send(js.dump());
        }
        else
        {
            // 有可能两用户不连接在同一个服务器上，要查看toid是否在线
            User user = _userModel.query(id);
            if (user.getState() == "online")
            {
                _redis.publish(id, js.dump());
            }
            else
            {
                // 发送离线消息
                _offMsgModel.insert(id, js.dump());
            }
        }
    }
}
//从redis消息队列中获取订阅消息
void ChatService::handlerRedisSubscribeMessage(int userid,string msg)
{
    
    lock_guard<mutex> lock(_connMutex);
    auto it =_userConnMap.find(userid);
    if(it!=_userConnMap.end())
    {
        it->second->send(msg);
        return ;
    }
    //存储用户的离线消息
    _offMsgModel.insert(userid,msg);
}