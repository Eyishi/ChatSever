#include "groupmodel.hpp"

// 创建群聊
bool GroupModel::createGroup(Group &group)
{
    // 1.组装sql语句
    char sql[1024] = {0};
    sprintf(sql, "insert into allgroup(groupname,groupdesc) value('%s','%s')",
            group.getName().c_str(), group.getDesc().c_str());
    MySQL mysql;
    if (mysql.connect())
    {
        if (mysql.update(sql))
        {
            group.setId(mysql_insert_id(mysql.getConnection()));
            return true;
        }
    }
    return false;
}
// 加入群聊
void GroupModel::addGroup(int userid, int groupid, string role)
{
    // 1.组装sql语句
    char sql[1024] = {0};
    sprintf(sql, "insert into groupusers value(%d,%d,'%s');",
            userid, groupid, role.c_str());
    MySQL mysql;
    if (mysql.connect())
    {
        mysql.update(sql);
    }
}
// 查询用户所在群组信息
vector<Group> GroupModel::queryGroups(int userid)
{
    /*
    1.先根据userid在groupusers表中查询出该用户所属群组消息
    2.在根据群组信息，查询属于该群组的所有用户的userid，并且和user表进行夺标联合查询，查出用户的详细信息
    */
    // 1.组装sql语句
    char sql[1024] = {0};
    sprintf(sql, "select a.id,a.groupname,a.groupdesc from allgroup a inner join \
    groupuser b on a.id = b.groupid where b.userid = %d",
            userid);
    vector<Group> vec;
    MySQL mysql;
    if (mysql.connect())
    {
        MYSQL_RES *res = mysql.query(sql);
        if (res != nullptr)
        {
            // 把userid用户的所有离线消息放入vec中
            MYSQL_ROW row;
            while ((row = mysql_fetch_row(res)) != nullptr)
            {
                Group group;
                group.setId(atoi(row[0]));
                group.setName(row[1]);
                group.setDesc(row[2]);
                vec.push_back(group);
            }
            mysql_free_result(res);
        }
    }

        // 查询用户所在群组的所有用户信息
        for (Group &group : vec)
        {
            sprintf(sql, "select a.id,a.name,a.state,b.grouprole  from user a inner join \
                groupuser b on b.userid = a.id where b.groupid = %d",
                    group.getId());
            MYSQL_RES *res = mysql.query(sql);
            if (res != nullptr)
            {
                MYSQL_ROW row;
                while ((row = mysql_fetch_row(res)) != nullptr)
                {
                    //cout<<"logggggg"<<endl;
                    GroupUser users;
                    users.setId(atoi(row[0]));
                    users.setName(row[1]);
                    users.setState(row[2]);
                    users.setRole(row[3]);
                    group.getUsers().push_back(users);
                }
                mysql_free_result(res);
            }
        }
    return vec;
}
// 根据指定groupid查询群组用户id列表，除了userid自己，主要用户群聊业务给群组其他成员群发消息
vector<int> GroupModel::queryGroupUsers(int userid, int groupid)
{
     char sql[1024] = {0};
    sprintf(sql, "select userid from groupuser where groupid = %d and userid != %d",groupid,userid);
    vector<int> vec;
    MySQL mysql;
    if (mysql.connect())
    {
        MYSQL_RES *res = mysql.query(sql);
        if (res != nullptr)
        {
            // 把userid用户的所有离线消息放入vec中
            MYSQL_ROW row;
            while ((row = mysql_fetch_row(res)) != nullptr)
            {
                
                vec.push_back(atoi(row[0]));
            }
            mysql_free_result(res);
        }
    }
    return vec;
}