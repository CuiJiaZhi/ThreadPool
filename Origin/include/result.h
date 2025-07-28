#ifndef __RESULT_H
#define __RESULT_H

#include <memory>

#include "task.h"
#include "sem.h"

#if 0   //// 1
// 接收任务返回值的对象
class Task;         // Task类型的前置声明
class Result
{
public:
    // 构造函数
    Result(std::shared_ptr<Task> task, bool isVaild = true);

    // 析构函数
    ~Result() = default;

    // 删除拷贝构造函数
    Result(const Result&) = delete;
    // 删除赋值重载运算符
    Result& operator=(const Result&) = delete;

    // set()，获取任务执行完的返回值并存入成员变量any_中
    void set(Any any);

    // get()，用户获取任务的返回值
    Any get();

private:
    Any any_;                       // 任务的返回值
    Semaphore sem_;                 // 线程通信信号量
    std::shared_ptr<Task> task_;    // 指向需要获取返回值的任务对象
    std::atomic_bool isVaild_;      // 返回值是否有效
};
#endif

#if 1   //// 2
// 更安全的实现方式
/*
    通过以下设计避免悬垂指针：
        - ​​引入Impl类​​Result和Task通过std::shared_ptr<Impl>间接交互
        - ​​弱引用打破循环​​Task持有Impl的弱引用（std::weak_ptr<Impl>），通过lock()安全访问

    通过​​中间层Impl类​​和​​弱引用机制​​，实现了：
        - 解耦​​：Task与Result通过Impl间接交互，职责分离
        - 安全​​：弱引用 + lock()自动处理对象失效，避免悬垂指针
    
    在需要严格生命周期控制的异步框架（如线程池、事件驱动架构）中优先采用此方案；通用场景可保留原始设计，但需显式管理Result生命周期
*/
class Impl 
{
public:
    Impl(bool isVaild = true);

    // 禁止拷贝
    Impl(const Impl&) = delete;
    Impl& operator=(const Impl&) = delete;

    // 设置结果
    void set(Any any);

    // 获取结果
    Any get();

    // 检查有效性
    bool isValid() const;

private:
    Any any_;
    Semaphore sem_;
    bool isValid_;
    bool isGet_;               // 是否已经调用get()方法
};

// 前置声明
class Task;      
// 接收任务返回值的对象
class Result
{
public:
    // 默认构造函数
    Result() = default;
    Result(std::shared_ptr<Task> task, bool isVaild = true);

    // 禁止拷贝
    Result(const Result&) = delete;
    Result& operator=(const Result&) = delete;

    // 获取结果
    Any get();

    // 检查有效性
    bool isValid() const;

private:
    std::shared_ptr<Impl> impl_;
};
#endif

#endif