#ifndef __SEM_H
#define __SEM_H

#include <condition_variable>
#include <mutex>
#include <atomic>

// 自定义信号量 -- 互斥锁 + 条件变量
class Semaphore
{
public:
    // 构造函数
    Semaphore(size_t count = 0);

    // 析构函数
    ~Semaphore();

    // 获取信号量资源，P操作
    void wait();

    // 增加信号量资源，V操作
    void post();

private:
    std::atomic_bool isExit_;
    size_t resourceCount_;          // 资源计数
    std::mutex mtx_;                // 互斥锁
    std::condition_variable cond_;  // 条件变量
};

#endif