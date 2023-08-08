
#[[
aux_source_directory(. SRC_LIST)
当新添加 xxx.cc 文件时，由于该 CMakeLists.txt 未发生修改，所以 SRC_LIST 不变，对应的 makefile 内容不变，可能导致链接错误。
解决方法1：将 build 目录中的内容全部删除 rm -r *，
          然后依次手动执行 cmake .. 命令（.. 为根 CMakeLists.txt 所在目录）和 make 命令（在 build 目录中执行）

解决方法2：使用如下（但每次新添加 xxx.cc 文件时都需要对如下进行修改） 
    set(SRC_LIST 
        mprpcapplication.cc 
        mprpcconfig.cc
        rpcprovider.cc 
    )
    这样，每次新添加 xxx.cc 文件后，该 CMakeLists.txt 发生了修改，SRC_LIST 变化，对应的 makefile 内容也跟着变化。
]]
aux_source_directory(. SRC_LIST)

add_library(mprpc ${SRC_LIST})

target_link_libraries(mprpc muduo_net muduo_base pthread zookeeper_mt)