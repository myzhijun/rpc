#pragma once

#include <muduo/net/TcpServer.h>
#include <muduo/net/InetAddress.h>
#include <muduo/net/EventLoop.h>
#include <muduo/net/TcpConnection.h>

#include <google/protobuf/service.h>
#include <google/protobuf/descriptor.h>

#include <string>
#include <unordered_map>

#include <functional>



/*
框架提供的专门负责发布 rpc 服务的网络对象类
RpcProvider 功能：
    通过 muduo 库接收 rpc 调用请求，并对其执行反序列化
    将反序列化后的请求匹配到所请求服务对象的某个 rpc 方法，并调用该 rpc 方法
    将 rpc 方法的执行结果进行序列化，然后将序列化后的结果传给 muduo 库，由 muduo 库将该结果发送到网络上，
        进而发送给请求者。
*/
class RpcProvider
{
public:
    // 这是框架提供给外部使用的，可以发布 rpc 方法的函数接口（参数需抽象化，不能具体到某一对象）
    // 接收派生自 ::google::protobuf::Service 的任何派生类对象
    void NotifyService(::google::protobuf::Service *service);

    // 启动 rpc 服务节点，开始提供 rpc 远程网络调用服务
    void Run();

private:
    // 组合 EventLoop，用于接收网络发送过来的 rpc 调用请求
    muduo::net::EventLoop m_eventLoop;

    // service 服务信息
    struct ServiceInfo
    {
        ::google::protobuf::Service *m_service;                                                    // 保存服务对象，用以调用服务方法
        std::unordered_map<std::string, const ::google::protobuf::MethodDescriptor *> m_methodMap; // 保存服务方法。key 为服务方法名。
    };

    // 存储注册成功的服务对象及其服务方法的所有信息
    std::unordered_map<std::string, ServiceInfo> m_serviceMap; // key 为服务对象名

    // 新的 socket 连接回调
    void OnConnection(const muduo::net::TcpConnectionPtr &);

    // 已建立连接用户的读写事件回调
    void OnMessage(const muduo::net::TcpConnectionPtr &, muduo::net::Buffer *, muduo::Timestamp);

    // Closure 的回调操作，用于序列化 rpc 响应，并将该响应通过 muduo 库发送到网络上
    void SendRpcResponse(const muduo::net::TcpConnectionPtr &, google::protobuf::Message *);
};