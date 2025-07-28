#include "thread.h"

size_t Thread::generateId_ = 0;

// 构造函数
Thread::Thread(ThreadFunc func)
    : func_(func)
    , threadId_(generateId_++)
{}

// 析构函数
Thread::~Thread() 
{}

// 启动线程
void Thread::start() {
    // 创建线程用来执行线程函数
    std::thread t(func_, threadId_);

    // 设置分离线程
    t.detach();
}

// 获取线程ID
size_t Thread::getId() const {
    return threadId_;
}