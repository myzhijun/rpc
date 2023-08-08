#include <iostream>
#include <string>
#include <vector>
#include "friend.pb.h"
#include "mprpcapplication.h"
#include "rpcprovider.h"
#include "logger.h"

class FriendService : public fixbug::FriendServiceRpc
{
public:
    std::vector<std::string> GetFriendsList(uint32_t userid)
    {
        std::cout << "do GetFriendsList service! userid: " << userid << std::endl;
        std::vector<std::string> vec;
        vec.push_back("gao yang");
        vec.push_back("liu hong");
        vec.push_back("wang shuo");
        return vec;
    }

    void GetFriendsList(::google::protobuf::RpcController *controller,
                        const ::fixbug::GetFriendsListRequest *request,
                        ::fixbug::GetFriendsListResponse *response,
                        ::google::protobuf::Closure *done)
    {

        uint32_t userid = request->userid();

        std::vector<std::string> friendsList = GetFriendsList(userid);

        fixbug::ResultCode *code = response->mutable_result();
        code->set_errcode(0);
        code->set_errmsg("");

        for (std::string &name : friendsList)
        {
            std::string *pfriend = response->add_friends();
            *pfriend = name;
        }

        // 执行回调操作 --- 执行响应数据的序列化和网络发送（都是由框架来完成的）
        done->Run();
    }
};

int main(int argc, char **argv)
{
    LOG_ERR("ddddd");
    LOG_INFO("aaaa");

    MprpcApplication::Init(argc, argv);

    RpcProvider provider;
    provider.NotifyService(new FriendService());

    provider.Run();

    return 0;
}