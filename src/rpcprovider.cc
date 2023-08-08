#include "rpcprovider.h"
#include "mprpcapplication.h"

#include "rpcheader.pb.h"
#include "logger.h"
#include "zookeeperutil.h"

/*
Q：框架（具体为 RpcProvider）接收到一个 rpc 调用请求（包含服务名 + 方法名 + 方法参数）时，如何知道要调用应用程序的哪个服务对象的哪个 rpc 方法？
答：需要一张表来记录服务对象及其发布的所有服务方法。该表的内容由 NotifyService() 来完成。
    UserService 服务对象    Login 方法          Register 方法
    FriendService 服务对象  AddFriend 方法      DelFriend 方法      GetFriendList 方法
    如此，当框架（具体为 RpcProvider）接收到一个 rpc 调用请求时，通过查该表找到所请求的服务对象及其相应的 rpc 方法，然后可调用该 rpc 方法来处理该请求。
        例如，RpcProvider 通过 muduo 库接收到一个 login 请求（包含服务名 + 方法名 + 方法参数），在完成 login 请求的反序列化操作后，
              获知该请求包含的 服务名+方法名+方法参数，然后据 服务名+方法名 来查表，找到 callee 提供的对应服务及其对应的方法，
              然后 RpcProvider 调用 callee 提供的该服务的对应方法来处理该 login 请求。
*/

/*
service_name => service 描述
                    => 一个 *service，指向一个服务对象，用以调用服务方法
                    => method_name => method 方法对象（一个服务对象或内含多个服务方法）

从而得到 struct ServiceInfo {...};
*/
void RpcProvider::NotifyService(::google::protobuf::Service *service)
{

    ServiceInfo service_info;

    // 获取服务对象 service 的描述信息
    const ::google::protobuf::ServiceDescriptor *pServiceDesc = service->GetDescriptor();

    /*
    // The name of the service, not including its containing scope.
    const std::string& name() const;

    获取服务对象 service 的名字

    要求 #include<google/protobuf/descriptor.h>
    */
    std::string service_name = pServiceDesc->name();

    /*
      // The number of methods this service defines.
      int method_count() const;

      获取服务对象 service 中的方法的数量
    */
    int methodCnt = pServiceDesc->method_count();

    std::cout << "service_name: " << service_name << std::endl;
    LOG_INFO("service_name: %s", service_name.c_str());

    for (int i = 0; i < methodCnt; ++i)
    {
        /*
        // Gets a MethodDescriptor by index, where 0 <= index < method_count().
        // These are returned in the order they were defined in the .proto file.
        const MethodDescriptor* method(int index) const;

        // 获取服务对象 service 的指定下标的服务方法的描述（抽象描述）
        */
        const ::google::protobuf::MethodDescriptor *pMethodDesc = pServiceDesc->method(i);

        /*
        // Name of this method, not including containing scope.
        const std::string& name() const;
        */
        const std::string method_name = pMethodDesc->name();

        service_info.m_methodMap.insert({method_name, pMethodDesc});

        std::cout << "method_name: " << method_name << std::endl;
        LOG_INFO("method_name: %s", method_name.c_str());
    }

    service_info.m_service = service;
    m_serviceMap.insert({service_name, service_info});
}

void RpcProvider::Run()
{

    // 读取配置文件中的 rpcserver 信息
    std::string ip = MprpcApplication::GetInstance().GetConfig().Load("rpcserverip");
    uint16_t port = stoi(MprpcApplication::GetInstance().GetConfig().Load("rpcserverport"));

    /*
    /// Constructs an endpoint with given ip and port.
    /// @c ip should be "1.2.3.4"
    InetAddress(StringArg ip, uint16_t port, bool ipv6 = false);
    */
    muduo::net::InetAddress address(ip, port);

    // 创建 TcpServer 对象
    muduo::net::TcpServer server(&m_eventLoop, address, "RpcProvider");

    /*
    绑定连接回调方法和消息读写回调方法 --- 使用 muduo 库的好处：分离了网络代码和业务代码
    OnConnection() 是 class RpcProvider 的一个成员函数，需借助一个对象才能调用之，故使用 bind() 将其绑定到 this 指针。
    OnMessage() 同理。

    std::bind 需要 #include <functional>
    */

    server.setConnectionCallback(std::bind(&RpcProvider::OnConnection, this, std::placeholders::_1)); // 不断查看定义，得到回调函数要求 void (const TcpConnectionPtr&)
    server.setMessageCallback(std::bind(&RpcProvider::OnMessage, this, std::placeholders::_1,
                                        std::placeholders::_2, std::placeholders::_3)); // 不断查看定义，得到回调函数要求 void(const TcpConnectionPtr &, Buffer *, Timestamp)

    // 设置 muduo 库的线程数量（muduo 库会自动分发 I/O 线程 和 工作线程）
    // 线程数 n >= 2 时，一个线程为 I/O 线程（接收新用户的连接，生成相应的连接 clientfd 分发为工作线程），其余线程为工作线程。
    // 线程数 n == 1 时，该线程既是 I/O 线程，也是工作线程。
    server.setThreadNum(4);

    // 把当前 rpc 节点上要发布的服务全部注册到 zk 上，从而让 rpc client 可以从 zk 上发现服务。
    // session timeout：30s  -- zkclient 的网络I/O线程会在 1/3 * timeout 时间定期地向 zkserver 发送 ping 消息来作为心跳消息
    ZkClient zkCli;
    zkCli.Start();

    // service_name 对应永久性节点    method_name 对应临时性节点
    for (auto &sp : m_serviceMap)
    {
        // sp.first => service_name ，路径名格式：/service_name，例如 /UserServiceRpc
        std::string service_path = "/" + sp.first;
        zkCli.Create(service_path.c_str(), nullptr, 0);

        for (auto &mp : sp.second.m_methodMap)
        {
            // 路径名格式：/service_name/method_name，例如 /UserServiceRpc/Login，存储当前这个 rpc 服务节点主机的 IP + Port
            std::string method_path = service_path + "/" + mp.first;

            char method_path_data[128] = {0};
            sprintf(method_path_data, "%s:%d", ip.c_str(), port);

            // ZOO_EPHEMERAL 表示 znode 节点是一个临时性节点
            zkCli.Create(method_path.c_str(), method_path_data, strlen(method_path_data), ZOO_EPHEMERAL);
        }
    }

    // rpc 服务端准备启动，打印信息
    std::cout << "RpcProvider start service at ip:" << ip << " port:" << port << std::endl;

    // 启动网络服务
    server.start();

    // 会阻塞当前线程，这将使得 zkCli 不会被析构，从而保持着 zkclient 和 zkserver 的连接，
    // 进而使得 zkserver 上的临时性节点得以存在。
    m_eventLoop.loop();
}

// 新的 socket 连接回调
void RpcProvider::OnConnection(const muduo::net::TcpConnectionPtr &conn)
{

    // rpc 请求相当于一个短连接的请求，类似 HTTP。服务器处理完请求并发送完响应后，就会主动断开连接。
    if (!conn->connected()) // 和 rpc client 的连接断开了
    {
        conn->shutdown();
    }
}

/*
在框架内部，RpcProvider 和 RpcConsumer 协商好二者通信所用的 protobuf 数据类型，包含有 service_name + method_name + method_args
单纯的 UserServiceLoginzhang san123456 无法从中提取出 UserService、Login、zhang san、123456
    故定义 proto 的对应 message 类型，以进行数据头的序列化和反序列化（method_args 已在 user.proto 中定义过）
        （数据头：service_name + method_name + method_args_size。
            之所以使用 method_args_size，是考虑到 TCP 粘包问题（当前请求的 method_args 有可能与下一个请求的数据混在一起））

为从 UserServiceLoginzhang san123456 中获知数据头，故引入数据头长度 header_size，有：
header_size(4 个字节) + header_str + method_args_str
    例如 16UserServiceLoginzhang san123456  -- 16 指示 header_str 中的字符个数

收到一串字符流后，先读取 4 个字节获取 header_size，然后根据 header_size 获取 header_str，
然后从 header_str 中提取出 service_name、method_name、method_args_size，接着读取 method_args_size 个字符得到 method_args。


=》rpcheader.proto（不与 user.proto 写在一起的原因是，user.proto 是 caller 使用的；
    而 rpcheader.proto 是 RpcProvider 使用的。谁使用，则谁定义。）
*/

// 已建立连接用户的读写事件回调
// 如果远程有一个 rpc 服务的调用请求，那么 OnMessage 方法就会响应
void RpcProvider::OnMessage(const muduo::net::TcpConnectionPtr &conn, muduo::net::Buffer *buffer, muduo::Timestamp)
{
    // 从网络中接收到的远程 rpc 调用请求的字符流
    std::string recv_buf = buffer->retrieveAllAsString();

    // 从字符流中读取前 4 个字节的内容
    uint32_t header_size = 0;

    // std::string::copy ： https://cplusplus.com/reference/string/string/copy/
    // 将 recv_buf 的前四个字节的二进制内容直接读取到 header_size 中
    recv_buf.copy((char *)&header_size, 4, 0);

    // 根据 header_size 读取数据头的原始字符流，反序列化数据，得到 rpc 请求的详细信息
    std::string rpc_header_str = recv_buf.substr(4, header_size);

    mprpc::RpcHeader rpcHeader;
    std::string service_name;
    std::string method_name;
    uint32_t args_size;

    if (rpcHeader.ParseFromString(rpc_header_str))
    {
        // 数据头反序列化成功
        service_name = rpcHeader.service_name();
        method_name = rpcHeader.method_name();
        args_size = rpcHeader.args_size();
    }
    else
    {
        // 数据头反序列化失败
        std::cout << "rpc_header_str:" << rpc_header_str << " parse error!" << std::endl;
        return;
    }

    // 获取 rpc 方法参数的字符流数据
    std::string args_str = recv_buf.substr(4 + header_size, args_size);

    // 打印调试信息
    std::cout << "============================================" << std::endl;
    std::cout << "header_size: " << header_size << std::endl;
    std::cout << "rpc_header_str: " << rpc_header_str << std::endl;
    std::cout << "service_name: " << service_name << std::endl;
    std::cout << "method_name: " << method_name << std::endl;
    std::cout << "args_str: " << args_str << std::endl;
    std::cout << "============================================" << std::endl;

    // 获取 service 对象和 method 对象
    auto service_ite = m_serviceMap.find(service_name);
    if (service_ite == m_serviceMap.end())
    {
        std::cout << service_name << " is not exist!" << std::endl;
        return;
    }

    auto method_ite = service_ite->second.m_methodMap.find(method_name);
    if (method_ite == service_ite->second.m_methodMap.end())
    {
        std::cout << method_name << " is not exist!" << std::endl;
        return;
    }

    ::google::protobuf::Service *service = service_ite->second.m_service;    // 获取 service 服务对象，如 new UserService()
    const ::google::protobuf::MethodDescriptor *method = method_ite->second; // 获取 method 方法对象，如 Login

    /*
    void Login(::google::protobuf::RpcController *controller,
            const ::fixbug::LoginRequest *request,
            ::fixbug::LoginResponse *response,
            ::google::protobuf::Closure *done)

    在 Login() 的参数中，request 指向一个反序列化后的 LoginRequest 对象。
    */

    // 生成 rpc 方法调用的 request 请求参数（并进行反序列化） 和 response 响应参数
    //   此时的 args_str 仍是字符流数据，尚未进行反序列化操作
    ::google::protobuf::Message *request = service->GetRequestPrototype(method).New();
    if (!request->ParseFromString(args_str))
    {
        std::cout << "request parse error, content:" << args_str << std::endl;
        return;
    }

    ::google::protobuf::Message *response = service->GetResponsePrototype(method).New();

    // 给下面的 method 方法调用，绑定一个 Closure 回调函数 -- 详细用法需追踪 Closure 的定义
    /*
    template <typename Class, typename Arg1, typename Arg2>
    inline Closure* NewCallback(Class* object, void (Class::*method)(Arg1, Arg2),
                                Arg1 arg1, Arg2 arg2) {
    return new internal::MethodClosure2<Class, Arg1, Arg2>(
        object, method, true, arg1, arg2);
    }
    该 NewCallback() 中的 object 指向一个 class 对象，该 class 内含的成员函数 method  要求两个参数。
    */
    ::google::protobuf::Closure *done = ::google::protobuf::NewCallback<RpcProvider,
                                                                        const muduo::net::TcpConnectionPtr &,
                                                                        ::google::protobuf::Message *>(this, &RpcProvider::SendRpcResponse, conn, response);

    // 在框架上根据远端的 rpc 请求，调用当前 rpc 节点上发布的方法
    // new UserService().Login(controller, request, response, done)
    service->CallMethod(method, nullptr, request, response, done);
}

// Closure 的回调操作，用于序列化 rpc 响应，并将该响应通过 muduo 库发送到网络上
void RpcProvider::SendRpcResponse(const muduo::net::TcpConnectionPtr &conn, ::google::protobuf::Message *response)
{
    std::string response_str;
    if (response->SerializeToString(&response_str)) // response 进行序列化
    {
        // 序列化成功后，通过 muduo 库把 rpc 方法的执行结果发送到网络中，进而发送给 rpc 方法的调用方。
        conn->send(response_str);
    }
    else
    {
        std::cout << "serialize response_str error!" << std::endl;
    }

    conn->shutdown(); // 模拟 HTTP 的短连接，由 rpcprovider 主动断开连接
}
