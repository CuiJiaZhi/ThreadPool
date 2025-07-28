#ifndef __THREAD_H
#define __THREAD_H

#include <iostream>
#include <functional>
#include <thread>

// 线程类型
class Thread
{
public:
    // 线程执行函数对象类型
    using ThreadFunc = std::function<void(size_t)>;

    // 构造函数
    Thread(ThreadFunc func);

    // 析构函数
    ~Thread();

    // 启动线程
    void start();

    // 获取线程id
    size_t getId() const;

private:
    ThreadFunc func_;           // 线程执行函数
    static size_t generateId_; 
    size_t threadId_;           // 线程id
};

#endif