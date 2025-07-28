#ifndef __THREADOPT_H__
#define __THREADOPT_H__

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
    Thread(ThreadFunc func, const std::string& name = "")
        : func_(func)
        , threadId_(generateId_++)
        , name_(name.empty() ? "Thread-" + std::to_string(threadId_) : name)
    {}

    // 析构函数
    ~Thread() = default;

    // 启动线程
    void start() {
        // 创建线程用来执行线程函数
        std::thread t(func_, threadId_);

        // 设置分离线程
        t.detach();
    }

    // 获取线程id
    size_t getId() const {
        return threadId_;
    }

    // 获取线程名称
    const std::string& getName() const {
        return name_;
    }

private:
    ThreadFunc func_;           // 线程执行函数
    std::string name_;          // 线程名称
    size_t threadId_;           // 线程id
    static size_t generateId_;  // 线程静态id
};

// 静态成员变量类外初始化
size_t Thread::generateId_ = 0;

#endif