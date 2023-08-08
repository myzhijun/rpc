/*
#include <iostream>
#include <string>


UserService 原是一个本地服务，提供了两个【进程内的本地方法】Login 和 GetFriendLists

class UserService
{
public:
    bool Login(std::string name, std::string pwd)
    {
        std::cout << "doing local service: Login" << std::endl;
        std::cout << "name:" << name << " pwd:" << pwd << std::endl;
        return false;
    }
};

int main()
{
    UserService us;

    // 该 UserService::Login 调用仅限于当前 main 进程内部，其他进程（本机上的，或其他机器上的）无法调用之。
    // 若想其他进程（本机上的，或其他机器上的）可调用之，需将该方法设置为一个 rpc 方法。
    us.Login("zhang san", "123456");

    return 0;
}

*/

// 为将 UserService::Login 处理为一个 rpc 方法，需要完成其 【方法名 + 方法参数 + 方法返回值】 的序列化和反序列化
// 由于本项目使用 prorobuf 来完成数据的序列化和反序列化（也可使用其他技术，如 Json、xml），故此引入一个 user.prpto。

#include <iostream>
#include <string>
#include "user.pb.h"
#include "mprpcapplication.h"
#include "rpcprovider.h"

class UserService : public fixbug::UserServiceRpc // 用在 rpc 服务发布端（即 rpc 服务提供者）
{
public:
    bool Login(std::string name, std::string pwd)
    {
        std::cout << "doing local service: Login" << std::endl;
        std::cout << "name:" << name << " pwd:" << pwd << std::endl;
        return true;
    }

    bool Register(uint32_t id, std::string name, std::string pwd)
    {
        std::cout << "doing local service: Register" << std::endl;
        std::cout << "id:" << id << " name:" << name << " pwd:" << pwd << std::endl;
        return true;
    }

    /*
    重写基类 UserServiceRpc 的虚函数 UserServiceRpc::Login
    下面这些方法都是框架自动调用的：
        1. caller   ===>   Login(LoginRequest) 请求  => muduo =>  callee
        2. callee   ===>   Login(LoginRequest) 请求  => 匹配到下面这个重写的 Login 方法上
            对于 caller 发送过来的请求，框架接收后会自动匹配到下述的对应处理方法并进行调用之
            （RpcProvider 将 muduo 库传过来的请求自动分发给 LoginRequest，并调用该 Login() 来处理该请求。
            请求处理结束后得到响应，该响应由 RpcProvider 通过 muduo 库发送到网络中，进而发送到 caller。
            ）。
    */
    void Login(::google::protobuf::RpcController *controller,
               const ::fixbug::LoginRequest *request,
               ::fixbug::LoginResponse *response,
               ::google::protobuf::Closure *done)
    {
        // 框架给应用上报了请求参数 LoginRequest，应用获取相应数据来执行本地业务
        std::string name = request->name();
        std::string pwd = request->pwd();

        // 执行本地业务
        bool login_result = Login(name, pwd);

        // 将本地业务的执行结果（包括错误码、错误消息、业务返回值）写入响应
        fixbug::ResultCode *code = response->mutable_result();
        code->set_errcode(0);
        code->set_errmsg("");
        response->set_sucess(login_result);

        // 执行回调操作 --- 执行响应数据的序列化和网络发送（都是由框架来完成的）
        done->Run();
    }

    void Register(::google::protobuf::RpcController *controller,
                  const ::fixbug::RegisterRequest *request,
                  ::fixbug::RegisterResponse *response,
                  ::google::protobuf::Closure *done)
    {
        uint32_t id = request->id();
        std::string name = request->name();
        std::string pwd = request->pwd();

        bool register_result = Register(id, name, pwd);

        fixbug::ResultCode *code = response->mutable_result();
        code->set_errcode(0);
        code->set_errmsg("");
        response->set_sucess(register_result);

        done->Run();
    }
};

int main(int argc, char **argv)
{

    // 调用框架的初始化操作。命令行：provider -i config.conf
    // （程序启动时需读取配置文件，如服务器的 IP 和 Port、ZooKeeper 的 IP 和 Port，因此需要 argc 和 argv）
    MprpcApplication::Init(argc, argv);

    // provider 是一个 rpc 网络服务对象。把 UserService 对象发布到 rpc 节点上。
    // 可能有多人请求 rpc 服务，所以 RpcProvider 需支持高并发，故使用 muduo 库来实现。
    RpcProvider provider;
    provider.NotifyService(new UserService());

    // 启动一个 rpc 服务发布节点。Run 以后，进程进入阻塞状态，等待远程的 rpc 调用请求
    provider.Run();

    return 0;
}