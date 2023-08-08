#include <iostream>
#include <string>
#include <unistd.h>
#include "mprpcapplication.h"

MprpcConfig MprpcApplication::m_config; // m_config 的初始化

void MprpcApplication::Init(int argc, char **argv)
{
    if (argc != 3)
    {
        std::cout << "Usage: " << argv[0] << " -i <configfile>" << std::endl;
        exit(EXIT_FAILURE);
    }

    int c = 0;
    std::string config_file;

    // getopt ：https://blog.csdn.net/afei__/article/details/81261879
    while ((c = getopt(argc, argv, "i:")) != -1)
    {
        switch (c)
        {
        case 'i':
            config_file = optarg;
            break;
        default:
            std::cout << "Usage: " << argv[0] << " -i <configfile>" << std::endl;
            exit(EXIT_FAILURE);
        }
    }

    // 开始加载配置文件: rpcserver_ip=  rpcserver_port   zookeeper_ip=  zookepper_port=

    m_config.LoadConfigFile(config_file.c_str());

    // std::cout << "rpcserverip: " << m_config.Load("rpcserverip") << std::endl;
    // std::cout << "rpcserverport: " << m_config.Load("rpcserverport") << std::endl;
    // std::cout << "zookeeperip: " << m_config.Load("zookeeperip") << std::endl;
    // std::cout << "zookeeperport: " << m_config.Load("zookeeperport") << std::endl;
}

MprpcApplication &MprpcApplication::GetInstance()
{
    static MprpcApplication app;
    return app;
}

MprpcConfig &MprpcApplication::GetConfig()
{
    return m_config;
}