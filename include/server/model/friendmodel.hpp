#ifndef FRIENDMODEL_H
#define FRIENDMODEL_H

#include"db.hpp"
#include"user.hpp"
#include<vector>
using namespace std;

//维护好友信息的操作接口方法
class FriendModel
{
public:
    //添加好友关系
    void insert(int userid,int friendid);

    //返回用户好友列表  friend的名称，是否在线
    vector<User> query(int userid);

};

#endif