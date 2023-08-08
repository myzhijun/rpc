#pragma once

#include "mprpcconfig.h"

#include "mprpcchannel.h"
#include "mprpccontroller.h"

// mprpc 框架的基础类（框架只有一个，故使用单例模式），负责框架的一些初始化操作
class MprpcApplication
{
public:
    static void Init(int argc, char **argv);
    static MprpcApplication &GetInstance();

    static MprpcConfig &GetConfig();

private:
    // 由于要在 Init() 中访问 m_config，故将 m_config 设置为 static
    static MprpcConfig m_config;

    MprpcApplication() {}
    MprpcApplication(const MprpcApplication &) = delete;
    MprpcApplication &operator=(const MprpcApplication &) = delete;

    MprpcApplication(MprpcApplication &&) = delete;
    MprpcApplication &operator=(MprpcApplication &&) = delete;
};