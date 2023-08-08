#include "test.pb.h"
#include <iostream>
using namespace fixbug;

// g++ *.cc -o main -lprotobuf  // -lprotobuf 参数用于链接 protobuf 动态库

int main1()
{
    // LoginResponse rsp;
    // ResultCode* rc = rsp.mutable_result();
    // rc->set_errcode(1);
    // rc->set_errmsg("登录失败！！！");        // 登陆失败后无需后续处理

    GetFriendListsResponse rsp;
    ResultCode *rc = rsp.mutable_result();
    rc->set_errcode(0); // 登陆成功后才有后续处理

    User *user1 = rsp.add_friend_list();
    user1->set_name("zhang san");
    user1->set_age(20);
    user1->set_sex(User::MAN);

    User *user2 = rsp.add_friend_list();
    user2->set_name("li si");
    user2->set_age(22);
    user2->set_sex(User::MAN);

    std::cout << rsp.friend_list_size() << std::endl;

    for (int i = 0; i < rsp.friend_list_size(); ++i)
    {
        const User &user = rsp.friend_list(i);

        std::cout << user.name() << " " << user.age() << std::endl;
    }

    return 0;
}

int main()
{
    
    // 封装了 LoginRequest 对象的数据
    LoginRequest req;
    req.set_name("zhang san"); // 使用 bytes 进行存储时，直接存储为字节；使用 string 进行存储时，存储为字符。
    req.set_pwd("123456");

    /*
    无法将 LoginRequest 对象 直接发送到网络中，需先对其进行序列化处理，然后将序列化处理后的数据发送到网络中。
    序列化：对象 => 字符流或字节流
    反序列化：字符流或字节流 => 对象
    */

    // 对象数据的序列化 =》 char*
    std::string send_str;

    /*
    Serialize the message and store it in the given string.  All required fields must be set.

    bool SerializeToString(std::string* output) const;
    */
    if (req.SerializeToString(&send_str))
    {
        std::cout << send_str.c_str() << std::endl; // 输出：        zhang san123456 可看出 数据zhang san 和 数据123456 之间紧凑排列
    }

    // 从 send_str 反序列化得到一个 LoginRequest 对象
    LoginRequest reqB;
    if (reqB.ParseFromString(send_str))
    {
        std::cout << reqB.name() << std::endl;
        std::cout << reqB.pwd() << std::endl;
    }


    return 0;
}
