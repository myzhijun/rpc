#include "zookeeperutil.h"
#include <semaphore.h>
#include "mprpcapplication.h" // 用于获取 zk 服务器的 IP + Port
#include <iostream>

// 全局的 watcher --- zkserver 给 zkclient 的通知
// the global watcher callback function. When notifications are triggered this function will be invoked.
void global_watcher(zhandle_t *zh, int type, int state, const char *path, void *watcherCtx)
{
    if (type == ZOO_SESSION_EVENT) // 回调的消息类型（和 Session 相关的消息类型）
    {
        if (state == ZOO_CONNECTED_STATE) // ZOO_CONNECTED_STATE 表示 zkclient 和 zkserver 之间的 Session 创建成功
        {
            /*
            return the context for this handle.
            const void *zoo_get_context(zhandle_t *zh);
            */
            sem_t *sem = (sem_t *)zoo_get_context(zh);

            /*
            Post SEM.
            extern int sem_post (sem_t *__sem)
            */
            sem_post(sem);
        }
    }
}

ZkClient::ZkClient() : m_zhandle(nullptr)
{
}

ZkClient::~ZkClient()
{
    if (m_zhandle != nullptr)
    {
        zookeeper_close(m_zhandle); // 关闭句柄，释放资源
    }
}

// zkclient 启动连接 zkserver
void ZkClient::Start()
{
    // 欲连接到 zk 服务器，则需先获取 zk 服务器的 IP + Port
    // 项目配置位于 MprpcApplication 内，故 #include "mprpcapplication.h"
    std::string host = MprpcApplication::GetInstance().GetConfig().Load("zookeeperip");
    std::string port = MprpcApplication::GetInstance().GetConfig().Load("zookeeperport");
    std::string connstr = host + ":" + port;

    /*
    zookeeper_mt：多线程版本
    zookeeper 的 API 客户端程序提供了三个线程：
        ~ API 调用线程（即当前线程，会执行 zookeeper_init() ）
        ~ 网络 I/O 线程 -- 在 zookeeper_init() 中通过 pthread_create 创建而得（基于 poll，因为 zk 客户端无需支持高并发），专门负责网络 I/O操作
            由此可见，网络 I/O 操作并不是由 API 调用线程在执行 zookeeper_init() 时直接亲自完成的，而是交由了网络 I/O 线程来完成。
        ~ watcher 回调线程 -- zk客户端 收到 zk服务器 的响应后，会通过 pthread_create 新建一个线程来执行 global_watcher() 回调函数。

    由于 API调用线程 和 watcher 回调线程 是两个线程，故引入 semaphore 信号量来完成二者的线程间通信。
    （用于判断一个 zookeeper session 是否创建成功）

    create a handle to used communicate with zookeeper. This method creates a new handle and a zookeeper session that corresponds to that handle.
    Session establishment is asynchronous（异步的）, meaning that
        the session should not be considered established until (and unless) an event of state ZOO_CONNECTED_STATE is received.

    返回值：a pointer to the opaque zhandle structure.
        If it fails to create a new zhandle the function returns NULL and the errno variable indicates the reason.

    zookeeper_init() 函数执行返回之后，只能通过判断 handle 是否为 nullptr 来判断 handle 是否创建成功。
    至于 Session 是否建立成功，则暂时无法得知。因为 Session 建立是异步的，即 handle建立 和 Session建立 是同步的。

    当 zk 客户端收到了 zk服务器 发送来的响应后，watcher 回调线程会执行 global_watcher() 这个回调函数。
    （在 global_watcher() 中可判断 Session 是否建立成功）
    */
    m_zhandle = zookeeper_init(connstr.c_str(), global_watcher, 30000, nullptr, nullptr, 0);
    if (nullptr == m_zhandle)
    {
        std::cout << "zookeeper_init error!" << std::endl;
        exit(EXIT_FAILURE);
    }

    // 执行到此，说明是 m_zhandle handle 创建成功，但 Session 不一定创建成功。

    sem_t sem; // 引入信号量来判断 Session 是否创建成功

    /*
    Initialize semaphore object SEM to VALUE.  If PSHARED then share it with other processes.
    extern int sem_init (sem_t *__sem, int __pshared, unsigned int __value);
    */
    sem_init(&sem, 0, 0);

    /*
    set the context for this handle.
    void zoo_set_context(zhandle_t *zh, void *context);
    */
    zoo_set_context(m_zhandle, &sem);

    sem_wait(&sem); // Wait for SEM being posted.  ---- 阻塞等待 SEM 被传过来

    // 运行至此，说明 Session 也建立成功了。
    std::cout << "zookeeper_init success!" << std::endl;
}

// 在 zkserver 上根据指定的 path 创建 znode 节点
void ZkClient::Create(const char *path, const char *data, int datalen, int state)
{
    char path_buffer[128];
    int bufferlen = sizeof(path_buffer);

    // 先判断 path 对应的 znode节点 是否存在。如果存在，则无需重复创建；否则，新建之。
    int flag = zoo_exists(m_zhandle, path, 0, nullptr);

    if (ZNONODE == flag) // 表示 path 对应的 znode 节点不存在
    {
        // 创建指定path的znode节点了
        flag = zoo_create(m_zhandle, path, data, datalen,
                          &ZOO_OPEN_ACL_UNSAFE, state, path_buffer, bufferlen);
        if (flag == ZOK)
        {
            std::cout << "znode create success... path:" << path << std::endl;
        }
        else
        {
            std::cout << "flag:" << flag << std::endl;
            std::cout << "znode create error... path:" << path << std::endl;
            exit(EXIT_FAILURE);
        }
    }
}

// 根据参数指定的 znode 节点路径，获取 znode 节点的值
std::string ZkClient::GetData(const char *path)
{
    char buffer[64];
    int bufferlen = sizeof(buffer);
    int flag = zoo_get(m_zhandle, path, 0, buffer, &bufferlen, nullptr);
    if (flag != ZOK)
    {
        std::cout << "get znode error... path:" << path << std::endl;
        return "";
    }
    else
    {
        return buffer;
    }
}