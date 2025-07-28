#ifndef __THREADPOOL_H
#define __THREADPOOL_H

#include <iostream>
#include <queue>
#include <memory>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <chrono>
#include <unordered_map>

#include "any.h"
#include "result.h"
#include "sem.h"
#include "thread.h"

// 线程池模式
enum class PoolMode {
    MODE_FIXED,         // fixed模式
    MODE_CACHED         // cached模式
};

// 线程池类型
class ThreadPool
{
public:
    // 构造函数
    ThreadPool();

    // 析构函数
    ~ThreadPool();
    
    // 禁止用户对线程池进行拷贝构造/赋值
    ThreadPool(const ThreadPool&) = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;

    // 设置线程池工作模式
    void setMode(PoolMode mode = PoolMode::MODE_FIXED);

    // 定义任务队列中任务数量的上限值
    void setTaskQueMaxThreshold(size_t threadhold);

    // 定义线程池中线程数量的上限
    void setThreadSizeThreshold(size_t threadhold);

    // 提交任务（生产者，向任务队列中提交任务）
    Result submitTask(std::shared_ptr<Task> task);

    // 开启线程池
    void start(size_t initThreadSize = std::thread::hardware_concurrency());

private:
    // 定义线程执行函数（消费者，不断从任务队列中获取任务）
    void threadFunc(size_t threadId);

    // 检查线程池的运行状态
    bool checkRunningState() const;

private:
    /*
        - 若std::vector中存放Thread的裸指针，由于std::vector在进行析构时，
          会先调用元素的析构函数，再释放std::vector占用的内存空间，
          而Thread*是一个指针，无析构函数，因此std::vector在析构时，
          无法释放Thread*指针指向的内存空间，需要手动释放，这增加了出现错误的可能性

        - 有没有更好的解决方法？有的，使用智能指针
    */
    // std::vector<Thread*> threads_;              // 线程容器
    // std::vector<std::unique_ptr<Thread>> threads_; // 线程容器
    std::unordered_map<size_t, std::unique_ptr<Thread>> threads_;   // 线程容器
    size_t initThreadSize_;                        // 初始线程数量
    size_t threadSizeThreshold_;                   // 线程数量上限
    std::atomic_uint curThreadSize_;               // 当前线程池中线程的数量
    std::atomic_uint idleThreadSize_;              // 当前线程池中空闲线程的数量
    
    // std::queue<Task*> taskQue_                  // 任务队列
    /*
        - 注意：
            - 直接使用Task裸指针是欠考虑的，因为不能保证用户传入的Task对象的生命周期，
              可能会导致任务队列中的Task指针指向一个已经被释放的任务
        - 解决方法：
            - 使用智能指针
    */ 
    //// 任务队列
    std::queue<std::shared_ptr<Task>> taskQue_;    // 任务队列
    std::atomic_uint taskSize_;                    // 任务数量
    size_t taskQueMaxThreshold_;                   // 任务数量上限

    //// 互斥锁
    std::mutex taskQueMtx_;                        // 保证任务队列的线程安全

    //// 条件变量
    std::condition_variable taskQueNotFull_;       // 任务队列不满
    std::condition_variable taskQueNotEmpty_;      // 任务队列不空
    std::condition_variable exitCond_;             // 等待线程资源全部回收

    //// 原子操作
    std::atomic_bool isPoolRunning_;               // 当前线程池的运行状态

    PoolMode poolMode_;                            // 当前线程池工作模式
};

#endif
