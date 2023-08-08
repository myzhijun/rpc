#pragma once
#include <string>
#include <zookeeper/zookeeper.h>

// 封装的 zk 客户端类 -- 因为 zk 提供的是 C API
class ZkClient
{
public:
    ZkClient();
    ~ZkClient();

    // zkclient 启动连接 zkserver
    void Start();

    // 在 zkserver 上根据指定的 path 创建一个 znode 节点，并由 state 指明是永久节点还是临时节点
    // （state 默认值为 0，表示永久节点，对应 ephemeralOwner = 0x0）
    void Create(const char *path, const char *data, int datalen, int state = 0);

    // 根据参数指定的 znode 节点路径，获取 znode 节点的值
    std::string GetData(const char *path);

private:
    // zk 的客户端句柄
    zhandle_t *m_zhandle;
};