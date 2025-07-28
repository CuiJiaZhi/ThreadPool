#ifndef __THREADPOOLOPT_H__
#define __THREADPOOLOPT_H__

#include <iostream>
#include <queue>
#include <memory>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <chrono>
#include <unordered_map>
#include <future>

#include "threadOpt.h"
#include "logger.h"

const int TASK_MAX_THRESHOLD   = INT32_MAX;     // 最大任务量
const int THREAD_MAX_THRESHOLD = 1024;          // 线程池中最大线程数
const int THREAD_MAX_IDLE_TIME = 60;            // 提交任务超时时间，单位：s

// 线程池模式
enum class PoolMode {
    MODE_FIXED,         // fixed模式
    MODE_CACHED         // cached模式
};

// 线程池类型
class ThreadPool
{
public:
    // 线程池构造函数
    ThreadPool() 
        : initThreadSize_(0)
        , taskSize_(0)
        , idleThreadSize_(0)
        , curThreadSize_(0)
        , taskQueMaxThreshold_(TASK_MAX_THRESHOLD)
        , threadSizeThreshold_(THREAD_MAX_THRESHOLD)
        , poolMode_(PoolMode::MODE_FIXED)
        , isPoolRunning_(false)
    {}

    // 析构函数
    ~ThreadPool() {
        // 表示需要回收线程池资源
        isPoolRunning_ = false;

        // 等待线程池中所有线程返回
        std::unique_lock<std::mutex> lock(taskQueMtx_);

        // 将线程池中阻塞的线程全部唤醒
        taskQueNotEmpty_.notify_all();

        // 等待线程池中的所有线程执行完毕
        exitCond_.wait(lock, [&]()->bool{
            return threads_.size() == 0;
        });
    }
    
    // 禁止对线程池进行拷贝构造/赋值
    ThreadPool(const ThreadPool&) = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;

    // 设置线程池工作模式
    void setMode(PoolMode mode = PoolMode::MODE_FIXED) {
        if(checkRunningState()) {
            // 不允许线程池启动后进行设置
            return;
        }

        poolMode_ = mode;
    }

    // 定义任务队列中任务数量的上限值
    void setTaskQueMaxThreshold(size_t threshold) {
        if(checkRunningState()) {
            // 不允许线程池启动后进行设置
            return;
        }

        taskQueMaxThreshold_ = threshold;
    }

    // 定义线程池中线程数量的上限
    void setThreadSizeThreshold(size_t threadhold) {
        if(checkRunningState()) {
            // 不允许线程池启动后进行设置
            return;
        }

        if(poolMode_ == PoolMode::MODE_CACHED) {
            threadSizeThreshold_ = threadhold;
        }
    }

    // 提交任务（生产者，向任务队列中提交任务）
    // 使用可变参模板编程，可接收任意任务函数与任意数量的参数
    // 将提交的任意任务统一封装成线程池可处理的void()类型任务，同时保证任务执行和返回值获取的安全性。这是实现线程池任务调度机制的经典模式
    /*
        任务提交 --> 任务包装 --> 队列存储
    */
    template<typename taskFunc, typename... Args>
    auto submitTask(taskFunc&& func, Args&&... args) -> std::future<decltype(func(args...))> {
        // 推导返回值类型
        // 基于具体表达式的编译时类型推导
        using retType = decltype(func(args...));

        // 任务包装
        // 使用std::shared_ptr确保std::packaged_task在任务执行完毕前不会被销毁
        auto task = std::make_shared<std::packaged_task<retType()>>(
            std::bind(std::forward<taskFunc>(func), std::forward<Args>(args)...)
        );

        // 获取任务返回值
        std::future<retType> result = task->get_future();

        // 获取锁
        std::unique_lock<std::mutex> lock(taskQueMtx_);

        // 等待任务队列未满，含超时判断机制，防止submitTask的调用线程一直阻塞
        if(!taskQueNotFull_.wait_for(lock, std::chrono::seconds(1), [&]()->bool{
            return taskQue_.size() < taskQueMaxThreshold_;
        })) {
            // 判定超时，任务提交失败
            LOG_INFO() << "Task queue is full, submit task timed out";

            auto task = std::make_shared<std::packaged_task<retType()>>(
                []()->retType{ return retType(); }
            );

            (*task)();

            // 返回当前类型的0值
            return task->get_future();
        }

        // 若队列未满，则向任务队列中添加任务
        // 通过lambda将std::packaged_task二次封装为void()类型的任务，通过lambda包装实现了类型擦除
        // 将用户提交的任务（函数 + 参数）封装成一个无参数、无返回值的Task对象，并放入线程池的任务队列中，等待工作线程取出执行
        taskQue_.emplace([task](){
            (*task)();
        });

        // 任务数量+1
        taskSize_++;

        // 通知其它线程任务队列不为空
        taskQueNotEmpty_.notify_all();

        // cached模式下，根据任务数量和空闲线程的数量，判断是否需要创建新的线程
        if(
            poolMode_ == PoolMode::MODE_CACHED &&   // cached模式
            taskSize_ > idleThreadSize_  &&         // 任务队列中的任务数量大于空闲线程的数量
            curThreadSize_ < threadSizeThreshold_   // 线程池中线程数量小于上限值
        ) 
        {
            // 生成新线程名称
            std::string threadName = "CachedThread-" + std::to_string(curThreadSize_);

            // 创建新线程
            auto obj = std::make_unique<Thread>(
                std::bind(&ThreadPool::threadFunc, this, std::placeholders::_1), 
                threadName
            );
            int threadId = obj->getId();
            threads_.emplace(threadId, std::move(obj));

            // 启动新的线程
            threads_[threadId]->start();

            curThreadSize_++;
            idleThreadSize_++;

            LOG_INFO() << "Created new thread: " << threadName;
        }

        return result;
    }

    // 开启线程池
    void start(size_t initThreadSize = std::thread::hardware_concurrency(), const std::string& threadNamePrefix = "PoolThread") {
        // 设置线程池的运行状态
        isPoolRunning_ = true;

        // 初始线程个数
        initThreadSize_ = initThreadSize;
        curThreadSize_ = initThreadSize;

        // 创建线程对象
        for(size_t i = 0; i < initThreadSize_; ++i) {
            // 生成线程名称
            std::string threadName = threadNamePrefix + "-" + std::to_string(i);

            // 创建Thread线程对象时，将线程执行函数给到创建的Thread对象
            auto obj = std::make_unique<Thread>(
                std::bind(&ThreadPool::threadFunc, this, std::placeholders::_1), 
                threadName
            );
            int threadId = obj->getId();
            threads_.emplace(threadId, std::move(obj));
        }

        // 启动所有线程
        for(size_t i = 0; i < initThreadSize_; ++i) {
            threads_[i]->start();

            idleThreadSize_++;          // 记录空闲线程的数量
        }

        LOG_INFO() << "Created " << initThreadSize << " initial threads with prefix: " << threadNamePrefix;
    }

private:
    // 定义线程执行函数，消费者，不断从任务队列中获取任务
    void threadFunc(size_t threadId) {
        // 获取当前线程名称
        auto&& thread = threads_[threadId];
        LOG_INFO() << "Thread " << thread->getName() << " started";

        // 记录当前时间
        auto lastTime = std::chrono::high_resolution_clock().now();

        // 线程不断循环，从任务队列中取出任务
        // 等待所有任务执行完成后，才可以回收线程池资源
        for(;;) {
            Task task;
            {
                // 获取锁
                std::unique_lock<std::mutex> lock(taskQueMtx_);

                LOG_INFO() << "Thread " << thread->getName() << " attempting to get task...";

                while(taskQue_.size() == 0) {
                    if(!isPoolRunning_) {
                        //// 回收线程池资源
                        LOG_INFO() << "Thread " << thread->getName() << " exiting";

                        // 回收当前线程，将线程对象从线程容器中删除
                        threads_.erase(threadId);

                        // 通知析构函数中的wait
                        exitCond_.notify_all();
                        
                        return;
                    }

                    // cached模式下，线程空闲时间超过60s，则回收空闲的线程
                    // 超过initThreadSize_数量的线程需要进行超时回收
                    if(poolMode_ == PoolMode::MODE_CACHED) {
                        if(std::cv_status::timeout == taskQueNotEmpty_.wait_for(lock, std::chrono::seconds(1))) {
                            // 条件变量超时返回
                            // 获取当前时间
                            auto nowTime = std::chrono::high_resolution_clock().now();
                            auto duration = std::chrono::duration_cast<std::chrono::seconds>(nowTime - lastTime).count();
                            if(
                                duration >= THREAD_MAX_IDLE_TIME &&     // 60s超时
                                curThreadSize_ > initThreadSize_        // 线程池中的线程数量大于线程初始数量
                            ) {
                                LOG_INFO() << "Thread " << thread->getName() << " timed out and exiting";

                                // 回收当前线程，将线程对象从线程容器中删除
                                threads_.erase(threadId);
                                
                                curThreadSize_--;
                                idleThreadSize_--;

                                return;
                            }
                        }
                    }
                    else {
                        // 等待任务队列不为空
                        // 在循环中检查条件，防止虚假唤醒
                        taskQueNotEmpty_.wait(lock);
                    }
                }

                // 线程准备处理任务，线程空闲数量减1
                idleThreadSize_--;

                LOG_INFO() << "Thread " << thread->getName() << " get task success";

                // 从任务队列的队头取出任务
                task = taskQue_.front();
                // 出队
                taskQue_.pop();

                // 任务数-1
                taskSize_--;

                // 若任务队列中仍然有任务，通知其它消费者从任务队列中取任务
                // 不仅让生产者通知消费者，也让消费者之间相互通知
                if(taskQue_.size() > 0) {
                    taskQueNotEmpty_.notify_all();
                }

                // 通知生产者任务队列未满，可以向任务队列提交任务
                taskQueNotFull_.notify_all();
            }

            // 当前线程执行该任务
            if(task != nullptr) {  
                LOG_INFO() << "Thread " << thread->getName() << " executing task";

                // 检查函数包装器是否为空，即未绑定任何可调用对象
                task();
            }

            // 线程任务完成，线程空闲数量加1
            idleThreadSize_++;

            LOG_INFO() << "Thread " << thread->getName() << " task completed";

            // 更新时间
            lastTime = std::chrono::high_resolution_clock::now();
        }
    }

    // 检查线程池的运行状态
    bool checkRunningState() const {
        return isPoolRunning_;
    }

private:
    std::unordered_map<size_t, std::unique_ptr<Thread>> threads_;   // 线程容器
    size_t initThreadSize_;                                         // 初始线程数量
    size_t threadSizeThreshold_;                                    // 线程数量上限
    std::atomic_uint curThreadSize_;                                // 当前线程池中线程的数量
    std::atomic_uint idleThreadSize_;                               // 当前线程池中空闲线程的数量
    
    //// 任务
    using Task = std::function<void()>;

    //// 任务队列
    std::queue<Task> taskQue_;                                      // 任务队列
    std::atomic_uint taskSize_;                                     // 任务数量
    size_t taskQueMaxThreshold_;                                    // 任务数量上限

    //// 互斥锁
    std::mutex taskQueMtx_;                                         // 保证任务队列的线程安全

    //// 条件变量
    std::condition_variable taskQueNotFull_;                        // 任务队列不满
    std::condition_variable taskQueNotEmpty_;                       // 任务队列不空
    std::condition_variable exitCond_;                              // 等待线程资源全部回收

    //// 原子操作
    std::atomic_bool isPoolRunning_;                                // 当前线程池的运行状态

    //// 线程池工作模式
    PoolMode poolMode_;                                             // 当前线程池工作模式
};

#endif
