#include <iostream>

#include "threadpoolOpt.h"

int sum(int a, int b) {
    return a + b;
}

int main()
{
    // 设置全局日志级别
    Logger::setLevel(LogLevel::INFO);

    // 定义线程池
    ThreadPool pool;
    // 设置为CACHED模式
    pool.setMode(PoolMode::MODE_CACHED);
    // 设置线程池中初始线程个数为3
    pool.start(3);

    std::future<int> r1 = pool.submitTask(sum, 1, 2);
    std::future<int> r2 = pool.submitTask(sum, 3, 4);
    std::future<int> r3 = pool.submitTask(sum, 5, 6);
    std::future<int> r4 = pool.submitTask(
        [](int a, int b) {
            return a + b;
        }, 7, 8
    );

    std::cout << r1.get() << "\n";
    std::cout << r2.get() << "\n";
    std::cout << r3.get() << "\n";
    std::cout << r4.get() << "\n";

    return 0;
}