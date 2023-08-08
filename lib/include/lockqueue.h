#pragma once
#include <queue>
#include <thread>
#include <mutex>              // 对应 Linux 的 pthread_mutex_t
#include <condition_variable> // 对应 Linux 的 pthread_condition_t

// 异步写日志的日志队列
template <typename T> // 由于使用了模板，故将成员函数的函数体写在该 .h 文件中
class LockQueue
{
public:
    // 多个 worker 线程都会写日志到 queue 中
    void Push(const T &data)
    {
        // 有日志则直接写入到 queue 中，此时不考虑 queue 为满的情况
        std::lock_guard<std::mutex> lock(m_mutex);
        m_queue.push(data);
        m_condvariable.notify_one(); // 只有一个线程从缓冲队列中取日志，故使用 notify_one()；若有多个线程从缓冲队列中取日志，则使用 notify_all()。
    }

    // 一个独立的线程从 queue 中读取日志，并将日志通过磁盘I/O 写入到日志文件中
    T Pop()
    {
        std::unique_lock<std::mutex> lock(m_mutex);
        while (m_queue.empty()) // 使用 while 来防止线程的虚假唤醒
        {
            // 缓冲队列 queue 为空，则线程进入 wait 状态
            m_condvariable.wait(lock);
        }

        T data = m_queue.front();
        m_queue.pop();
        return data;
    }

private:
    std::queue<T> m_queue;
    std::mutex m_mutex;
    std::condition_variable m_condvariable;
};