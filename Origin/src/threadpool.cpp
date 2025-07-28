#include "threadpool.h"

const int TASK_MAX_THRESHOLD   = INT32_MAX;
const int THREAD_MAX_THRESHOLD = 1024;
const int THREAD_MAX_IDLE_TIME = 60;    // 单位：s

// 线程池构造函数
ThreadPool::ThreadPool() 
    : initThreadSize_(0)
    , taskSize_(0)
    , idleThreadSize_(0)
    , curThreadSize_(0)
    , taskQueMaxThreshold_(TASK_MAX_THRESHOLD)
    , threadSizeThreshold_(THREAD_MAX_THRESHOLD)
    , poolMode_(PoolMode::MODE_FIXED)
    , isPoolRunning_(false)
{}

// 线程池析构函数
/*
    因为在构造函数中，没有在堆上开辟内存空间（即没有new对象）,
    因此，析构函数的无需释放内存空间

    等待所有任务执行完成后，才可以回收线程池资源
*/
ThreadPool::~ThreadPool() 
{
    // 成员变量的修改，表示需要回收线程池资源
    isPoolRunning_ = false;

    // 等待线程池中所有线程返回
    std::unique_lock<std::mutex> lock(taskQueMtx_);

    // 将线程池中阻塞的线程全部唤醒
    taskQueNotEmpty_.notify_all();

    exitCond_.wait(lock, [&]()->bool{
        return threads_.size() == 0;
    });
}

// 设置线程池工作模式
void ThreadPool::setMode(PoolMode mode) {
    if(checkRunningState()) {
        // 不允许线程池启动后进行设置
        return;
    }
    poolMode_ = mode;
}

// 定义任务队列中任务数量的上限值
void ThreadPool::setTaskQueMaxThreshold(size_t threshold) {
    if(checkRunningState()) {
        return;
    }

    taskQueMaxThreshold_ = threshold;
}

// 定义线程池中线程数量的上限 
void ThreadPool::setThreadSizeThreshold(size_t threadhold) {
    if(checkRunningState()) {
        return;
    }

    if(poolMode_ == PoolMode::MODE_CACHED) {
        threadSizeThreshold_ = threadhold;
    }
}

// 提交任务（生产者：向任务队列中添加任务）
Result ThreadPool::submitTask(std::shared_ptr<Task> task) {
    // 获取锁
    std::unique_lock<std::mutex> lock(taskQueMtx_);

    // 等待任务队列未满，含超时判断机制，防止submitTask的调用线程一直阻塞
    if(!taskQueNotFull_.wait_for(lock, std::chrono::seconds(1), [&]()->bool{
        return taskQue_.size() < taskQueMaxThreshold_;
    })) 
    {
        // 判定超时
        // std::cerr << "task queue is full, submit task fail!\n";

        // 返回错误
        // 任务（task）对象的生命周期要与Result对象的生命周期相同
        return Result(task, false);
    }

    // 若队列未满，则向任务队列中添加任务
    taskQue_.emplace(task);

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
        // 创建新线程
        auto ptr = std::make_unique<Thread>(std::bind(&ThreadPool::threadFunc, this, std::placeholders::_1));
        int threadId = ptr->getId();
        threads_.emplace(threadId, std::move(ptr));

        // 启动新的线程
        threads_[threadId]->start();

        curThreadSize_++;
        idleThreadSize_++;

        // std::cout << "Create New Thread!\n";
    }

    // 返回结果
    // 任务（task）对象的生命周期要与Result对象的生命周期相同
    return Result(task);
}

// 启动线程池
void ThreadPool::start(size_t initThreadSize) {
    // 设置线程池的运行状态
    isPoolRunning_ = true;

    // 初始线程个数
    initThreadSize_ = initThreadSize;
    curThreadSize_ = initThreadSize;

    // 创建线程对象
    for(size_t i = 0; i < initThreadSize_; ++i) {
        // 创建Thread线程对象时，将线程执行函数给到创建的Thread对象
        auto ptr = std::make_unique<Thread>(std::bind(&ThreadPool::threadFunc, this, std::placeholders::_1));
        int threadId = ptr->getId();
        
        // 此处emplace内部会直接调用std::unordered_map的构造函数，效率高
        threads_.emplace(threadId, std::move(ptr));
    }

    // 启动所有线程
    for(size_t i = 0; i < initThreadSize_; ++i) {
        threads_[i]->start();

        idleThreadSize_++;          // 记录空闲线程的数量
    }

    // std::cout << "Create " << initThreadSize << " Init Thread\n";
}

// 线程执行函数，消费者：从任务队列中取出任务执行
void ThreadPool::threadFunc(size_t threadId) {
    // 记录当前时间
    auto lastTime = std::chrono::high_resolution_clock().now();

    // 线程不断循环，从任务队列中取出任务
    // 等待所有任务执行完成后，才可以回收线程池资源
    for(;;) {
        std::shared_ptr<Task> task;
        {
            // 获取锁
            /*
                - 当一个线程在持有互斥锁时结束（无论是正常结束还是异常终止），该互斥锁会被自动释放
                - 虽然线程在持有锁时直接退出，但C++的RAII（资源获取即初始化）机制会确保锁被释放
            */
            std::unique_lock<std::mutex> lock(taskQueMtx_);

            // std::cout << "Tid: " << std::this_thread::get_id() << " Attempt To Get Task...\n";

            while(taskQue_.size() == 0) {
                if(!isPoolRunning_) {
                    //// 回收线程池资源
                    // 回收当前线程，将线程对象从线程容器中删除
                    // 需要映射关系，获取当前线程具体是线程容器中的哪个线程
                    threads_.erase(threadId);
                    // std::cout << "threadid: " << std::this_thread::get_id() << " exit!\n";

                    // 通知析构函数中的wait
                    exitCond_.notify_all();
                    
                    
                    // 持有锁时return，即线程持有互斥锁时直接退出的情况
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
                            duration >= THREAD_MAX_IDLE_TIME &&     /*60s超时*/
                            curThreadSize_ > initThreadSize_        /*线程池中的线程数量大于线程初始数量*/
                        ) {
                            // 回收当前线程，将线程对象从线程容器中删除
                            threads_.erase(threadId);
                            
                            curThreadSize_--;
                            idleThreadSize_--;

                            // std::cout << "threadid: " << std::this_thread::get_id() << " exit!\n";
                            
                            // 持有锁时return，即线程持有互斥锁时直接退出的情况
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

            // std::cout << "Tid: " << std::this_thread::get_id() << " Get Task Success!\n";

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
        /*
            任务执行不能包含在互斥锁的作用域内，
            否则会导致当前线程在任务执行完后才会把互斥锁释放
        */
        if(task != nullptr) {  
            task->exec();   // 执行任务；将任务的返回值通过setVal()方法给到Result
        }

        // 线程任务完成，线程空闲数量加1
        idleThreadSize_++;

        // 更新时间
        lastTime = std::chrono::high_resolution_clock::now();
    }
}

bool ThreadPool::checkRunningState() const {
    return isPoolRunning_;
}
