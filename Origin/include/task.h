#ifndef __TASK_H
#define __TASK_H

#include <mutex>
#include "any.h"
#include "result.h"

#if 0   //// 1
// 任务抽象基类
/*
    用户可自定义任务类型，从抽象基类Task继承而来，重写run方法，
    特性：继承、多态
*/
class Result;           // Result类型的前置声明
class Task
{
public:
    // 构造函数
    Task();

    // 析构函数
    ~Task() = default;
    
    void exec();
    
    void setResult(Result* res);

    // 纯虚函数
    virtual Any run() = 0;

private:
    /*
        - 使用裸指针即可，不要使用强智能指针（shared_ptr），会有循环引用问题，
          因为Result中有指向Task的强智能指针（shared_ptr）
    */
    Result* result_;
};
#endif

#if 1   //// 2
// 前置声明
class Impl;
// 任务抽象基类，用户可自定义任务类型，从抽象基类Task继承而来，重写run方法，
class Task
{
public:
    // 构造函数
    Task() = default;

    // 析构函数
    virtual ~Task() = default;
    
    // 执行任务
    void exec();
    
    void setResult(const std::weak_ptr<Impl>& impl);

    // 纯虚函数
    virtual Any run() = 0;

private:
    std::weak_ptr<Impl> impl_;    // 弱引用结果
};
#endif

#endif