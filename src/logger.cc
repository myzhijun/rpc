#include <time.h>
#include <iostream>
#include "logger.h"

Logger::Logger()
{
    // 启动专门的写日志线程
    std::thread writeLogTask([&]()
                             {
		while (true) {
			// 获取当前的日期，然后取日志信息，写入相应的日志文件当中 a+（追加模式）
			time_t now = time(nullptr);

			tm *nowtm = localtime(&now);

			char file_name[128];
			sprintf(file_name, "%d-%d-%d-log.txt", nowtm->tm_year + 1900, nowtm->tm_mon + 1, nowtm->tm_mday);

			FILE *pf = fopen(file_name, "a+");
			if (pf == nullptr) {
				std::cout << "logger file : " << file_name << " open error!" << std::endl;
				exit(EXIT_FAILURE);
			}

			std::string msg = m_lckQue.Pop();

			char time_buf[128] = {0};
			sprintf(time_buf, "%d:%d:%d =>[%s] ",
			        nowtm->tm_hour,
			        nowtm->tm_min,
			        nowtm->tm_sec,
			        (m_loglevel == 1 ? "info" : "error"));		// TODO：该部分判断实现有误

			msg.insert(0, time_buf);
			msg.append("\n");
            
			fputs(msg.c_str(), pf);

            // 注意，直接使用 fclose(pf); 时，每向日志文件中写入一条日志后，都会关闭该日志文件。
            // 合理情况是：当 日志缓冲队列 lockqueue 为空时才关闭该日志文件，否则继续写入日志到该日志文件中。此处有待改进。
			fclose(pf);
		} });

    // 设置分离线程，类似守护线程
    writeLogTask.detach();
}

// 获取日志的单例
Logger &Logger::GetInstance( )
{
    static Logger logger;
    return logger;
}

// 设置日志级别
void Logger::SetLogLevel(LogLevel level)
{
    m_loglevel = level;
}

// 写日志到日志缓冲队列 lockqueue 中
void Logger::Log(std::string msg)
{
    m_lckQue.Push(msg);
}