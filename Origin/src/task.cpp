#include "task.h"

#if 0   //// 1
Task::Task()
    : result_(nullptr)
{}

void Task::exec() {
    if(result_ != nullptr) {
        // Any any = this->run();
        // result_->set(std::move(any));    // 多态

        result_->set(this->run());    // 多态
    }
}

void Task::setResult(Result* res) {
    result_ = res;
}
#endif

#if 1   //// 2
void Task::exec() {
    if(auto impl = impl_.lock()) {
        // 安全提升为强引用
        impl->set(std::move(this->run()));
    }
}

void Task::setResult(const std::weak_ptr<Impl>& impl) {
    impl_ = impl;
}
#endif