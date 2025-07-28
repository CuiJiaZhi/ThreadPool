#include "result.h"

#if 0   //// 1
// 构造函数
Result::Result(std::shared_ptr<Task> task, bool isVaild)
    : task_(task)
    , isVaild_(isVaild)
{
    task_->setResult(this);
}

// 获取任务执行完的返回值，存入成员变量any_中
void Result::set(Any any) {
    // any_中含有std::unique_ptr，因此使用std::move()
    any_ = std::move(any);

    // 资源计数+1
    // 通知结果已经就绪
    sem_.post();    
}

// 用户调用，获取任务的返回值
// 阻塞直至就绪
Any Result::get() {
    if(!isVaild_) {
        return "";
    }

    // 资源计数-1
    sem_.wait();                // 等待任务执行完成

    // any_中含有std::unique_ptr，因此使用std::move()
    return std::move(any_);
}
#endif

#if 1   //// 2
//// Impl实现
Impl::Impl(bool isValid)
    : isValid_(isValid)
    , isGet_(false)
{}

//
void Impl::set(Any any) {
    any_ = std::move(any);

    // 资源计数+1
    // 通知结果已经就绪
    sem_.post();
}

// 
Any Impl::get() {
    if(isGet_) {
        // 已经调用过get()，再次调用会抛出异常
        throw std::runtime_error("Result already retrieved");
    }

    // 资源计数-1
    sem_.wait();                // 等待任务执行完成

    isGet_ = true;              // 已经调用过get()

    return std::move(any_);
}

// 
bool Impl::isValid() const {
    return isValid_;
}

//// Result实现
Result::Result(std::shared_ptr<Task> task, bool isValid) 
    : impl_(std::make_shared<Impl>(isValid))
{
    if(task) {
        task->setResult(impl_);
    }
}

//
Any Result::get() {
    if(!impl_ || !impl_->isValid()) {
        throw std::runtime_error("Invalid result");
    }

    return impl_->get();
}

// 
bool Result::isValid() const {
    return impl_ && impl_->isValid();
}

#endif
