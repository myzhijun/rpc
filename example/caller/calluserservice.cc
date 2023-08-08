#include <iostream>
#include "mprpcapplication.h"
#include "user.pb.h"

int main(int argc, char **argv)
{
    // 整个程序启动以后，想使用 mprpc 框架来享受 rpc 服务调用，
    // 一定需要先调用框架的初始化函数（只初始化一次）
    MprpcApplication::Init(argc, argv);

    // 演示调用远程发布的 rpc 方法 Login
    /*
    UserServiceRpc_Stub(::PROTOBUF_NAMESPACE_ID::RpcChannel* channel);
    RpcChannel 是一个抽象类，无法定义类对象，需自定义其派生类，并重写其中的 CallMethod() 方法。
    */
    fixbug::UserServiceRpc_Stub stub(new MprpcChannel());

    // ---------------------------------

    // rpc 方法的请求参数
    fixbug::LoginRequest request;
    request.set_name("zhang san");
    request.set_pwd("123456");

    // rpc 方法的响应
    fixbug::LoginResponse response;

    // 发起 rpc 方法的调用 => 同步的 rpc 调用过程 MprpcChannel::callmethod
    MprpcController controller;
    stub.Login(&controller, &request, &response, nullptr); // RpcChannel->RpcChannel::callMethod 集中来做所有 rpc 方法调用的参数序列化和网络发送

    if (controller.Failed())
    {
        std::cout << controller.ErrorText() << std::endl;
    }
    else
    {
        // 一次 rpc 方法调用完成，获取调用的结果
        if (response.result().errcode() == 0)
        {
            std::cout << "rpc login response success:" << response.sucess() << std::endl;
        }
        else
        {
            std::cout << "rpc login response error : " << response.result().errmsg() << std::endl;
        }
    }

    // ---------------------------------

    // 演示调用远程发布的 rpc 方法 Register
    fixbug::RegisterRequest regRequest;
    regRequest.set_id(2000);
    regRequest.set_name("li si");
    regRequest.set_pwd("666");

    // rpc 方法的响应
    fixbug::RegisterResponse regResponse;

    // 以同步的方式发起 rpc 调用请求，等待返回结果
    stub.Register(&controller, &regRequest, &regResponse, nullptr);

    if (controller.Failed())
    {
        std::cout << controller.ErrorText() << std::endl;
    }
    else
    {
        // 一次 rpc 方法调用完成，获取调用的结果
        if (regResponse.result().errcode() == 0)
        {
            std::cout << "rpc register response success:" << regResponse.sucess() << std::endl;
        }
        else
        {
            std::cout << "rpc register response error : " << regResponse.result().errmsg() << std::endl;
        }
    }
    return 0;
}