项目代码工程目录
    bin：可执行文件
    build：cmake 编译生成的中间文件
    lib：项目编译后生成的库文件
    src：源文件
    test：测试代码
    example：框架代码使用范例
    CMakeLists.txt：顶层的 cmake 文件
    README.md：项目自述文件
    autobuild.sh：一键编译脚本


include 目录下的所有头文件为该框架所用到的头文件，最终需要拷贝到 lib 目录下，
与框架生成的静态库一起提供给用户使用。


谁提供了 rpc 服务，则谁负责定义一个 .proto 文件。


生成了 lib 目录下的 libmprpc.a 静态库。
    选择生成静态库而不是动态库，是因为 muduo 库是静态库，在生成动态库时，链接 muduo 库 出错。
    若要选择生成动态库，则需将 muduo 库重新编译成动态库进行链接。

    选择生成静态库的好处之一是 muduo 库已被包含在其中，使用该静态库时无需再配置 muduo 库环境。
    '''
    [build] /usr/bin/ld: /usr/local/lib/libmuduo_net.a(EventLoop.cc.o): relocation R_X86_64_TPOFF32 against `_ZN12_GLOBAL__N_118t_loopInThisThreadE' can not be used when making a shared object; recompile with -fPIC
    [build] /usr/bin/ld: /usr/local/lib/libmuduo_net.a(InetAddress.cc.o): relocation R_X86_64_TPOFF32 against `_ZL15t_resolveBuffer' can not be used when making a shared object; recompile with -fPIC
    [build] /usr/bin/ld: /usr/local/lib/libmuduo_base.a(Logging.cc.o): relocation R_X86_64_TPOFF32 against symbol `_ZN5muduo10t_errnobufE' can not be used when making a shared object; recompile with -fPIC
    [build] collect2: error: ld returned 1 exit status
    [build] make[2]: *** [src/CMakeFiles/mprpc.dir/build.make:161: /home/liu/rpc/lib/libmprpc.so] Error 1
    [build] make[1]: *** [CMakeFiles/Makefile2:149: src/CMakeFiles/mprpc.dir/all] Error 2
    [build] make: *** [Makefile:91: all] Error 2
    [proc] The command: /usr/local/bin/cmake --build /home/liu/rpc/build --config Debug --target all -j 6 -- exited with code: 2
    [driver] Build completed: 00:00:24.870
    [build] Build finished with exit code 2

    '''

一个单独的 service 需单独对应一个 .prpto 文件。
    例如 user.proto     friend.proto