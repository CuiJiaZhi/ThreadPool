#include <iostream>

#include "threadpool.h"
#include "logger.h"

//// 线程池用例
/*
    场景：
        - 计算1 + 2 + ... + 400000000的和
            - 线程1计算1 + 2 + ... + 100000000的和
            - 线程2计算10000001 + 10000002 + ... + 200000000的和
            - 线程3计算20000001 + 20000002 + ... + 300000000的和
            - 线程4计算30000001 + 30000002 + ... + 400000000的和
            - 主线程给每个线程分配计算，并合并线程1、2、3、4的计算结果
*/
class MyTask : public Task 
{
public:
    // 构造函数
    MyTask(int begin, int end)
        : begin_(begin)
        , end_(end)
    {}

    // 重写run()方法
    virtual Any run() override {
        unsigned long long sum = 0;
        for(int i = begin_; i <= end_; ++i) {
            sum += i; 
        }

        return sum;
    }

private:
    int begin_;
    int end_;
};

int main()
{
    // 设置全局日志级别
    Logger::setLevel(LogLevel::INFO);

    {
        // 线程池
        ThreadPool pool;

        // 设置线程池模式
        pool.setMode(PoolMode::MODE_CACHED);

        // 启动线程池
        pool.start(2);
        
        Result res1 = pool.submitTask(std::make_shared<MyTask>(1        , 100000000));
        Result res2 = pool.submitTask(std::make_shared<MyTask>(100000001, 200000000));
        Result res3 = pool.submitTask(std::make_shared<MyTask>(200000001, 300000000));
        Result res4 = pool.submitTask(std::make_shared<MyTask>(300000001, 400000000));

        unsigned long long r1 = res1.get().cast<unsigned long long>();
        unsigned long long r2 = res2.get().cast<unsigned long long>();
        unsigned long long r3 = res3.get().cast<unsigned long long>();
        unsigned long long r4 = res4.get().cast<unsigned long long>();

        std::cout << "Thread-1: " << r1 << "\n";
        std::cout << "Thread-2: " << r2 << "\n";
        std::cout << "Thread-3: " << r3 << "\n";
        std::cout << "Thread-4: " << r4 << "\n";
        std::cout << "Thread-1~4: " << r1 + r2 + r3 + r4 << "\n";
    }

    // getchar();

    return 0;
}
