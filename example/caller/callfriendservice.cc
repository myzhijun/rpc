#include <iostream>
#include "mprpcapplication.h"
#include "friend.pb.h"

int main(int argc, char **argv)
{
    MprpcApplication::Init(argc, argv);

    fixbug::FriendServiceRpc_Stub stub(new MprpcChannel());

    // rpc 方法的请求参数
    fixbug::GetFriendsListRequest request;
    request.set_userid(1000);

    // rpc 方法的响应
    fixbug::GetFriendsListResponse response;

    // 发起 rpc 方法的调用 
    MprpcController controller;
    stub.GetFriendsList(&controller, &request, &response, nullptr);

    /*
    对 response 进行下述处理的前提是 request 成功处理并成功发送到 callee，且 callee 成功处理了该 request 并返回了 response，
    并且 response 从 callee 成功发送到了 caller。这种情况过于理想化。
    例如在  mprpcchannel.cc 中的 return;，则此时的 response 为空，就不应该有下述处理。
    故引入 mprpccontroller 来存储一些控制信息，以便了解当前 rpc 方法的调用状态。
    由于 ::google::protobuf::RpcController 是一个抽象类，故需继承它来自定义之。
    */
    if (controller.Failed())
    {
        std::cout << controller.ErrorText() << std::endl;
    }
    else
    {

        if (0 == response.result().errcode())
        {
            std::cout << "rpc GetFriendsList response success!" << std::endl;
            int size = response.friends_size();
            for (int i = 0; i < size; ++i)
            {
                std::cout << "index: " << (i + 1) << " name: " << response.friends(i) << std::endl;
            }
        }
        else
        {
            std::cout << "rpc GetFriendsList response error : " << response.result().errmsg() << std::endl;
        }
    }

    return 0;
}