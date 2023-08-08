#pragma once
#include <unordered_map>
#include <string>

// 框架的用于读取配置文件的一个类
// 配置文件内容 rpcserverip   rpcserverport    zookeeperip   zookeeperport。
class MprpcConfig
{
public:
    // 负责加载并解析配置文件
    void LoadConfigFile(const char *config_file);

    // 查询某个配置项信息
    std::string Load(const std::string &key);

private:
    std::unordered_map<std::string, std::string> m_configMap;

    // 去掉字符串前后的空格
    void Trim(std::string &src_buf);
};