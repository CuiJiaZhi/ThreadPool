#include "sem.h"

// 构造函数
Semaphore::Semaphore(size_t count)
    : resourceCount_(count)
    , isExit_(false)
{}

// 析构函数
Semaphore::~Semaphore() {
    isExit_ = true;
}

// 获取信号量资源，P操作
void Semaphore::wait() {
    if(isExit_) {
        return;
    }

    // 获取互斥锁
    std::unique_lock<std::mutex> lock(mtx_);

    cond_.wait(lock, [&]()->bool{
        return resourceCount_ > 0;
    });

    // 资源计数-1
    resourceCount_--;
}

// 增加信号量资源，V操作
void Semaphore::post() {
    if(isExit_) {
        return;
    }

    // 获取互斥锁
    std::unique_lock<std::mutex> lock(mtx_);

    // 资源计数+1
    resourceCount_++;

    // 通知
    cond_.notify_all();
}