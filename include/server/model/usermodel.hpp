#ifndef USER_MODEL
#define USER_MODEL

#include"user.hpp"
//user表的数据操作类
class UserModel
{
public:
    //User表的增加
    bool insert(User&);

    //根据id表查询信息
    User query(int);

    //更新用户状态信息
    bool updateState(User &);

    //重置用户的状态信息
    void resetState();
private:
};

#endif