#include "mprpcchannel.h"
#include "rpcheader.pb.h"
#include "mprpcapplication.h"
#include "mprpccontroller.h"
#include "zookeeperutil.h"
#include <string>

#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <unistd.h>
#include <errno.h>

/*
对于 caller 而言，done 无用，直接传参 nullptr。
                controller 暂时用不到，也直接传参 nullptr。

消息数据的格式：header_size + service_name method_name args_size + args
                 header_size 根据 service_name method_name args_size 计算得到
*/
void MprpcChannel::CallMethod(const ::google::protobuf::MethodDescriptor *method,
                              ::google::protobuf::RpcController *controller,
                              const ::google::protobuf::Message *request,
                              ::google::protobuf::Message *response,
                              ::google::protobuf::Closure *done)
{

    const std::string method_name = method->name(); // 获取 method_name

    const ::google::protobuf::ServiceDescriptor *serviceDesc = method->service();
    const std::string service_name = serviceDesc->name(); // 获取 service_name

    // 获取参数 args 的序列化字符串长度 args_size
    uint32_t args_size = 0;
    std::string args_str;
    if (request->SerializeToString(&args_str))
    {
        args_size = args_str.size();
    }
    else
    {
        controller->SetFailed("serialize request error!");
        return;

        // 未引入 controller 时用下述语句处理
        // std::cout << "serialize request error!" << std::endl;
        // return;
    }

    mprpc::RpcHeader rpcHeader;
    rpcHeader.set_service_name(service_name);
    rpcHeader.set_method_name(method_name);
    rpcHeader.set_args_size(args_size);

    // 获取 rpcHeader 的序列化字符串长度 header_size
    uint32_t header_size = 0;
    std::string rpc_header_str;
    if (rpcHeader.SerializeToString(&rpc_header_str))
    {
        header_size = rpc_header_str.size();
    }
    else
    {
        controller->SetFailed("serialize rpc header error!");
        return;

        // 未引入 controller 时用下述语句处理
        // std::cout << "serialize rpc header error!" << std::endl;
        // return;
    }

    // 组织待发送的 rpc 请求的字符串
    std::string send_rpc_str;

    /** insert()
     *  @brief  Construct string initialized by a character %array.
     *  @param  __s  Source character %array.
     *  @param  __n  Number of characters to copy.
     *  @param  __a  Allocator to use (default is default allocator).
     *
     *  NB: @a __s must have at least @a __n characters, &apos;\\0&apos;
     *  has no special meaning.
     *
    basic_string(const _CharT* __s, size_type __n, const _Alloc& __a = _Alloc());


    *  @brief  Insert value of a string.
    *  @param __pos1  Iterator referencing location in string to insert at.
    *  @param __str  The string to insert.
    *  @return  Reference to this string.
    *  @throw  std::length_error  If new length exceeds @c max_size().
    *
    *  Inserts value of @a __str starting at @a __pos1.  If adding
    *  characters causes the length to exceed max_size(),
    *  length_error is thrown.  The value of the string doesn't
    *  change if an error is thrown.

    basic_string& insert(size_type __pos1, const basic_string& __str);
    */
    send_rpc_str.insert(0, std::string((char *)&header_size, 4)); // header_size。通过 insert() 将 header_size 恒置于 send_rpc_str 的起始。
    send_rpc_str += rpc_header_str;                               // rpcheader
    send_rpc_str += args_str;                                     // args

    // 打印调试信息
    std::cout << "============================================" << std::endl;
    std::cout << "send_rpc_str: " << send_rpc_str << std::endl;
    std::cout << "send_rpc_str size: " << send_rpc_str.size() << std::endl;
    std::cout << "header_size: " << header_size << std::endl;
    std::cout << "rpc_header_str: " << rpc_header_str << std::endl;
    std::cout << "service_name: " << service_name << std::endl;
    std::cout << "method_name: " << method_name << std::endl;
    std::cout << "args_str: " << args_str << std::endl;
    std::cout << "args_size: " << args_size << std::endl;
    std::cout << "============================================" << std::endl;

    // 使用 tcp 编程，完成 rpc 方法的远程调用

    int clientfd = socket(AF_INET, SOCK_STREAM, 0);
    if (-1 == clientfd)
    {
        char errtxt[512] = {0};
        sprintf(errtxt, "create socket error! errno:%d", errno);
        controller->SetFailed(errtxt);
        return;

        // 未引入 controller 时用下述语句处理
        // std::cout << "create socket error! errno: " << errno << std::endl;
        // exit(EXIT_FAILURE);
    }

    //--------------------------------
    //   读取配置文件 rpcserver 的信息
    // std::string ip = MprpcApplication::GetInstance().GetConfig().Load("rpcserverip");
    // uint16_t port = atoi(MprpcApplication::GetInstance().GetConfig().Load("rpcserverport").c_str());

    // rpc 服务调用方想调用 service_name 的 method_name 服务，需要查询 zk 上该服务所在的 IP + Port 信息
    ZkClient zkCli;
    zkCli.Start();

    //  /UserServiceRpc/Login
    std::string method_path = "/" + service_name + "/" + method_name;

    // 获取 IP + Port，如 127.0.0.1:8000
    std::string host_data = zkCli.GetData(method_path.c_str());

    if (host_data == "")
    {
        controller->SetFailed(method_path + " is not exist!");
        return;
    }

    int idx = host_data.find(":");
    if (idx == -1)
    {
        controller->SetFailed(method_path + " address is invalid!");
        return;
    }
    
    std::string ip = host_data.substr(0, idx);
    uint16_t port = atoi(host_data.substr(idx + 1, host_data.size() - idx).c_str());

    //--------------------------------

    // #include <arpa/inet.h>
    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    server_addr.sin_addr.s_addr = inet_addr(ip.c_str());

    // 连接 rpc 服务节点
    if (-1 == connect(clientfd, (struct sockaddr *)&server_addr, sizeof(server_addr)))
    {
        close(clientfd); // #include<unistd.h>

        char errtxt[512] = {0};
        sprintf(errtxt, "connect error! errno:%d", errno);
        controller->SetFailed(errtxt);
        return;

        // 未引入 controller 时用下述语句处理
        // std::cout << "connect error! errno: " << errno << std::endl;
        // exit(EXIT_FAILURE);
    }

    // 发送 rpc 请求
    if (-1 == send(clientfd, send_rpc_str.c_str(), send_rpc_str.size(), 0))
    {
        close(clientfd);

        char errtxt[512] = {0};
        sprintf(errtxt, "send error! errno:%d", errno);
        controller->SetFailed(errtxt);
        return;

        // 未引入 controller 时用下述语句处理
        // std::cout << "send error! errno: " << errno << std::endl;
        // exit(EXIT_FAILURE);
    }

    // 接收 rpc 请求的响应
    char recv_buf[1024] = {0};
    int recv_size = 0;
    if (-1 == (recv_size = recv(clientfd, recv_buf, 1024, 0)))
    {
        close(clientfd);

        char errtxt[512] = {0};
        sprintf(errtxt, "recv error! errno:%d", errno);
        controller->SetFailed(errtxt);
        return;

        // 未引入 controller 时用下述语句处理
        // std::cout << "recv error! errno: " << errno << std::endl;
        // exit(EXIT_FAILURE);
    }

    // 反序列化 rpc 调用的响应数据
    // std::string response_str(recv_buf, 0, recv_size); // bug 出现问题，recv_buf 中遇到 '\0' 后面的数据就存不下来了，导致反序列化失败
    // if (!response->ParseFromString(response_str))
    /*
    std::string::string ：https://cplusplus.com/reference/string/string/string/

        char recv_buf[10] = {0};
        recv_buf[0] = 'a';
        recv_buf[1] = 'a';
        recv_buf[2] = '\0';
        recv_buf[3] = 'a';
        recv_buf[4] = 'a';

        string response_str(recv_buf, 0, 5);
        cout << recv_buf << endl;	// 输出：aa

    */
    if (!response->ParseFromArray(recv_buf, recv_size))
    {
        close(clientfd);

        char errtxt[1080] = {0};
        sprintf(errtxt, "parse error! response_str:%s", recv_buf);
        controller->SetFailed(errtxt);
        return;

        // 未引入 controller 时用下述语句处理
        // std::cout << "parse error! response_str: " << errno << std::endl;
        // exit(EXIT_FAILURE);
    }
    close(clientfd);
}
