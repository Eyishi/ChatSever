#include "json.hpp"
#include <iostream>
#include <thread>
#include <string>
#include <vector>
#include <chrono>
#include <ctime>
using namespace std;
using json = nlohmann::json;

#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <semaphore.h>
#include <atomic>

#include "group.hpp"
#include "user.hpp"
#include "public.hpp"

// 记录当前系统登录的用户信息
User g_currentUser;

// 记录当前登录的用户的好友列表信息
vector<User> g_currentUserFriends;

// 记录当前登录用户的群组列表信息
vector<Group> g_currentUserGroupList;

// 控制主菜单页面程序
bool isMainMenuRuning = false;

// 用于读写线程之间的通信
sem_t rwsem;
// 记录登录状态
atomic_bool g_isLoginSuccess{false};

// 显示当前登录成功用户的基本信息
void showCurrentUserData();

// 接收线程
void readTaskHandler(int clientfd);

// 获取系统事件(聊天信息需要添加时间信息)
string getCurrentTime();

// 主聊天页面程序
void mianMenu(int clientfd);

void help(int fd = 0, string str = "");
void chat(int, string);
void addfriend(int, string);
void creategroup(int, string);
void addgroup(int, string);
void groupchat(int, string);
void loginout(int, string);
// 系统支持的客户端命令列表
unordered_map<string, string> commandMap =
    {
        {"help", "显示所有支持的命令，格式help"},
        {"chat", "一对一聊天，格式chat:friendid:message"},
        {"addfriend", "添加好友，格式addfriend:friendid"},
        {"creategroup", "创建群组，格式creategroup:groupname:groupdesc"},
        {"addgroup", "加入群组,格式addgroup:groupid"},
        {"groupchat", "群聊，格式groupchat:groupid:message"},
        {"loginout", "注销，格式loginout"}};
// 注册系统支持的客户端命令处理
unordered_map<string, function<void(int, string)>> commandHandlerMap =
    {
        {"help", help},
        {"chat", chat},
        {"addfriend", addfriend},
        {"creategroup", creategroup},
        {"addgroup", addgroup},
        {"groupchat", groupchat},
        {"loginout", loginout}};

void help(int, string)
{
    cout << "show command list" << endl;
    for (auto &p : commandMap)
    {
        cout << p.first << " : " << p.second << endl;
    }
    cout << endl;
}
void addfriend(int clientfd, string str)
{
    int friendid = atoi(str.c_str());
    json js;
    js["msgid"] = ADD_FRIEND_MSG;
    js["id"] = g_currentUser.getId();
    js["friendid"] = friendid;
    string buffer = js.dump();
    int len = send(clientfd, buffer.c_str(), strlen(buffer.c_str()) + 1, 0);
    if (-1 == len)
    {
        cerr << "send addfriend msg error->" << buffer << endl;
    }
}
void chat(int clientfd, string str)
{
    int idx = str.find(":");
    if (-1 == idx)
    {
        cerr << "chat command invalid!" << endl;
        return;
    }
    int friendid = atoi(str.substr(0, idx).c_str());
    string message = str.substr(idx + 1, str.size() - idx);
    json js;
    js["msgid"] = ONE_CHAT_MSG;
    js["id"] = g_currentUser.getId();
    js["name"] = g_currentUser.getName();
    js["toid"] = friendid;
    js["time"] = getCurrentTime();
    js["msg"] = message;
    string buffer = js.dump();

    int len = send(clientfd, buffer.c_str(), strlen(buffer.c_str()) + 1, 0);
    if (-1 == len)
    {
        cerr << "send chat msg error->" << buffer << endl;
    }
}
void creategroup(int clientfd, string str)
{
    int idx = str.find(":");
    if (-1 == idx)
    {
        cerr << "creategroup command invalid!" << endl;
        return;
    }
    string groupname = str.substr(0, idx);
    string groupdesc = str.substr(idx + 1, str.size() - idx);
    json js;
    js["msgid"] = CREATE_GROUP_MSG;
    js["id"] = g_currentUser.getId();
    js["groupname"] = groupname;
    js["groupdesc"] = groupdesc;
    string buffer = js.dump();

    int len = send(clientfd, buffer.c_str(), strlen(buffer.c_str()) + 1, 0);
    if (-1 == len)
    {
        cerr << "send creategroup msg error->" << buffer << endl;
    }
}

void addgroup(int clientfd, string str)
{
    int groupid = atoi(str.c_str());
    json js;
    js["msgid"] = ADD_GROUP_MSG;
    js["id"] = g_currentUser.getId();
    js["groupid"] = groupid;
    string buffer = js.dump();
    int len = send(clientfd, buffer.c_str(), strlen(buffer.c_str()) + 1, 0);
    if (-1 == len)
    {
        cerr << "send addgroup msg error->" << buffer << endl;
    }
}
// groupchat:groupid:message
void groupchat(int clientfd, string str)
{
    int idx = str.find(":");
    if (-1 == idx)
    {
        cerr << "groupchat command invalid!" << endl;
        return;
    }
    int groupid = atoi(str.substr(0, idx).c_str());
    string message = str.substr(idx + 1, str.size() - idx);
    json js;
    js["msgid"] = GROUP_CHAT_MSG;
    js["id"] = g_currentUser.getId();
    js["groupid"] = groupid;
    js["msg"] = message;
    js["time"] = getCurrentTime();
    string buffer = js.dump();

    int len = send(clientfd, buffer.c_str(), strlen(buffer.c_str()) + 1, 0);
    if (-1 == len)
    {
        cerr << "send groupchat msg error->" << buffer << endl;
    }
}
//"loginout" command handler groupid:message
void loginout(int clientfd, string str)
{
    json js;
    js["msgid"] = LOGINOUT_MSG;
    js["id"] = g_currentUser.getId();
    string buffer = js.dump();
    int len = send(clientfd, buffer.c_str(), strlen(buffer.c_str()) + 1, 0);
    if (-1 == len)
    {
        cerr << "send loginout msg error->" << buffer << endl;
    }
    else
    {
        isMainMenuRuning = false;
    }
}
void doLoginResponse(json &response)
{
    if (0 != response["errno"].get<int>()) // 登录失败
    {
        cerr << response["errmsg"] << endl;
        g_isLoginSuccess = false;
    }
    else // 登录成功
    {
        // 记录当前用户的id和name
        g_currentUser.setId(response["id"].get<int>());
        g_currentUser.setName(response["name"]);

        // 记录当前用户的好友列表

        if (response.contains("friends"))
        {
            vector<string> vec = response["friends"];
            for (string &str : vec)
            {
                json jsfriend = json::parse(str);
                User user;
                user.setId(jsfriend["id"].get<int>());
                user.setName(jsfriend["name"]);
                user.setState(jsfriend["state"]);
                g_currentUserFriends.push_back(user);
            }
        }

        // 记录当前用户的群组列表信息

        if (response.contains("groups"))
        {
            vector<string> vec1 = response["groups"];

            for (string &groupstr : vec1)
            {
                json jsgroup = json::parse(groupstr);
                Group group;
                group.setId(jsgroup["id"].get<int>());
                group.setName(jsgroup["groupname"]);
                group.setDesc(jsgroup["groupdesc"]);

                vector<string> vec2 = jsgroup["users"];
                for (auto &userstr : vec2)
                {
                    json jsuser = json::parse(userstr);
                    GroupUser user;
                    user.setId(jsuser["id"].get<int>());
                    user.setName(jsuser["name"]);
                    user.setState(jsuser["state"]);
                    user.setRole(jsuser["role"]);

                    group.getUsers().push_back(user);
                }

                g_currentUserGroupList.push_back(group);
            }
        }
        // 显示登录用户的基本信息
        showCurrentUserData();

        // 显示当前用户的离线消息
        if (response.contains("offlinemsg"))
        {
            vector<string> vec = response["offlinemsg"];
            for (auto &str : vec)
            {
                json js = json::parse(str);
                // 用户消息
                int msgtype = js["msgid"].get<int>();
                if (ONE_CHAT_MSG == msgtype)
                {
                    cout << js["time"].get<string>() << "[" << js["id"] << "]" << js["name"].get<string>()
                         << " said: " << js["msg"].get<string>() << endl;
                }
                // 群消息
                if (GROUP_CHAT_MSG == msgtype)
                {
                    cout << "群消息" << js["groupid"] << js["time"].get<string>()
                         << "[" << js["id"] << "]"
                         << " said: " << js["msg"].get<string>() << endl;
                }
            }
        }
        g_isLoginSuccess = true;
    }
}
// 处理注册响应
void doRegResponse(json &responsejs)
{
    if (0 != responsejs["errno"].get<int>()) // 注册失败
    {
        cerr << "已经存在,注册失败" << endl;
    }
    else // 注册成功
    {
        cout << "name register success,userid is" << responsejs["id"]
             << ",do not forget it!" << endl;
    }
}

// 接收线程
void readTaskHandler(int clientfd)
{
    for (;;)
    {
        char buffer[1024] = {0};
        int len = recv(clientfd, buffer, 1024, 0); // 阻塞了
        if (-1 == len || 0 == len)
        {
            close(clientfd);
            exit(-1);
        }
        // 接收Server转发的数据，反序列化json对象
        json js = json::parse(buffer);
        int msgtype = js["msgid"].get<int>();
        // 用户消息
        if (ONE_CHAT_MSG == msgtype)
        {
            cout << js["time"].get<string>() << "[" << js["id"] << "]" << js["name"].get<string>()
                 << " said: " << js["msg"].get<string>() << endl;
        }
        // 群消息
        if (GROUP_CHAT_MSG == msgtype)
        {
            cout << "群消息" << js["groupid"] << js["time"].get<string>() << "[" << js["id"] << "]"
                 << " said: " << js["msg"].get<string>() << endl;
        }
        if (LOGIN_MSG_ACK == msgtype)
        {
            doLoginResponse(js); // 处理登录响应的业务逻辑
            sem_post(&rwsem);    // 通知主线程，处理登录响应结果
            continue;
        }
        if (REG_MSG_ACK == msgtype)
        {
            doRegResponse(js);
            sem_post(&rwsem); // 通知主线程，处理登录响应结果
            continue;
        }
    }
}

// 主聊天页面程序
void mianMenu(int clientfd)
{
    help();
    char buffer[1024] = {0};
    while (isMainMenuRuning)
    {
        cin.getline(buffer, 1024);
        string commandbuf(buffer);
        string command; // 存储命令
        int idx = commandbuf.find(":");
        if (-1 == idx)
        {
            command = commandbuf;
        }
        else
        {
            command = commandbuf.substr(0, idx);
        }
        auto it = commandHandlerMap.find(command);
        if (it == commandHandlerMap.end())
        {
            cerr << "invalid input command!" << endl;
            continue;
        }
        // 调用相应命令的事件回调处理，mainMenu堆修改封闭，添加新功能不需要修改该函数
        it->second(clientfd, commandbuf.substr(idx + 1, commandbuf.size() - idx)); // 调用命令处理方法0
    }
}

void login(int clientfd) // 用户登录
{

    int id = 0;
    char pwd[50] = {0};
    cout << "userid:";
    cin >> id;
    cin.get();
    cout << "userpassword:";
    cin.getline(pwd, 50);

    json js;
    js["msgid"] = LOGIN_MSG;
    js["id"] = id;
    js["password"] = pwd;
    string request = js.dump();

    g_isLoginSuccess = false;

    int len = send(clientfd, request.c_str(), strlen(request.c_str()) + 1, 0);
    if (-1 == len)
    {
        cerr << "send login msg faild" << request << endl;
    }

    sem_wait(&rwsem); // 等待信号量，由子线程处理完登录的响应消息后，通知这里

    if (g_isLoginSuccess)
    {
        // 进入聊天主菜单界面
        isMainMenuRuning = true;
        mianMenu(clientfd);
    }
}
// 聊天客户端程序实现，main线程用作发送线程，子线程用作接收线程
int main(int argc, char **argv)
{
    if (argc < 3)
    {
        cerr << "command invalid! example ./ChatClient 127.0.0.1 6000" << endl;
        exit(-1);
    }

    // 解析通过命令行参数传递的ip和port
    char *ip = argv[1];
    uint16_t port = atoi(argv[2]);

    // 创建client端的socket
    int clientfd = socket(AF_INET, SOCK_STREAM, 0);
    if (-1 == clientfd)
    {
        cerr << "socket create faild" << endl;
        exit(-1);
    }

    // 填写client需要的server信息        ip+port

    sockaddr_in server;
    memset(&server, 0, sizeof(sockaddr_in));

    server.sin_family = AF_INET;
    server.sin_port = htons(port);
    server.sin_addr.s_addr = inet_addr(ip);

    // client 与server 连接
    if (-1 == connect(clientfd, (sockaddr *)&server, sizeof(sockaddr_in)))
    {
        cerr << "connect faild" << endl;
        close(clientfd);
        exit(-1);
    }
    // 初始化读写线程通信用的
    sem_init(&rwsem, 0, 0);

    // 连接成功，启动子线程
    std::thread readTask(readTaskHandler, clientfd);
    readTask.detach();

    // main线程用于接收用户输入，负责发送数据
    for (;;)
    {
        // 显示首页面菜单  登录 注册  退出
        cout << "=====================" << endl;
        cout << "1.login" << endl;
        cout << "2.register" << endl;
        cout << "3.quit" << endl;
        cout << "======================" << endl;
        cout << "choice";
        int choice = 0;
        cin >> choice;
        cin.get(); // 读掉缓冲区 残留的回车
        switch (choice)
        {
        case 1: // login
            login(clientfd);
            break;
        case 2: // register
        {
            char name[50] = {0};
            char pwd[50] = {0};
            cout << "username:";
            cin.getline(name, 50);
            cout << "userpwd:";
            cin.getline(pwd, 50);
            json js;
            js["msgid"] = REG_MSG;
            js["name"] = name;
            js["password"] = pwd;

            string request = js.dump();
            int len = send(clientfd, request.c_str(), strlen(request.c_str()) + 1, 0);
            if (-1 == len)
            {
                cerr << " send regmsg faild" << endl;
            }

            sem_wait(&rwsem); // 等待信号量，子线程处理完注册消息会通知
        }
        break;
        case 3: // quit
            close(clientfd);
            sem_destroy(&rwsem);
            exit(0);
        default:
            cerr << "incalid input!" << endl;
            break;
        }
    }
    return 0;
}
// 显示当前登录成功用户的基本信息
void showCurrentUserData()
{
    cout << "===========================login============================" << endl;
    cout << "current login user:" << endl;
    cout << "id   : " << g_currentUser.getId() << endl;
    cout << "name : " << g_currentUser.getName() << endl;
    cout << "--------------------------friends list---------------------------" << endl;
    if (!g_currentUserFriends.empty())
    {
        for (auto &it : g_currentUserFriends)
        {
            cout << it.getId() << " " << it.getName() << " " << it.getState() << endl;
        }
    }
    cout << "--------------------------group list---------------------------" << endl;
    if (!g_currentUserGroupList.empty())
    {
        for (auto &group : g_currentUserGroupList)
        {
            cout << group.getId() << " " << group.getName() << " " << group.getDesc() << endl;
            cout << "group users:" << endl;
            for (auto &user : group.getUsers())
            {
                cout << user.getId() << " " << user.getName() << " " << user.getState() << " "
                     << user.getRole() << endl;
            }
        }
    }
    cout << "============================================================" << endl;
    g_currentUserFriends.clear();
    g_currentUserGroupList.clear();
}

// 获取系统事件(聊天信息需要添加时间信息)
string getCurrentTime()
{
    auto tt = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    struct tm *ptm = localtime(&tt);
    char date[60] = {0};
    sprintf(date, "%02d-%02d-%02d %02d:%02d:%02d", (int)ptm->tm_year, (int)ptm->tm_mon + 1, (int)ptm->tm_mday, (int)ptm->tm_hour, (int)ptm->tm_min, (int)ptm->tm_sec);
    return string(date);
}