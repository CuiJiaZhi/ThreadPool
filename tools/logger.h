#ifndef __LOGGER_H__
#define __LOGGER_H__

#include <iostream>
#include <sstream>
#include <mutex>
#include <cstring>

#ifndef LOG_ENABLED
#define LOG_ENABLED 1  // 1: 启用, 0: 禁用
#endif

// 日志级别
enum class LogLevel {
    INFO,       // 普通信息
    DEBUG,      // 调试信息
    WARN,       // 警告信息
    ERROR,      // 错误信息
    FATAL,      // 致命信息
};

// 日志系统 - 懒汉式单例模式
class Logger {
public:
    // 设置全局日志级别
    static void setLevel(LogLevel level) {
        // 加锁
        std::lock_guard<std::mutex> lock(instance().mutex_);
        instance().logLevel_ = level;
    }

    // 获取日志单例实例
    static Logger& instance() {
        static Logger logger;
        return logger;
    }

    // 流式日志接口
    class LogStream {
    public:
        // 构造函数
        LogStream(Logger& logger, LogLevel level, const char* file, int line)
            : logger_(logger), level_(level)
        {
            // 获取文件名
            // 寻找字符串file中字符'/'最后一次出现的位置并返回指向该位置的指针
            const char* filename = strrchr(file, '/');
            filename = filename ? filename + 1 : file;

            //
            ss_ << LogLevelToString(level) << " "
                << filename << ":" << line << " | ";
        }

        // 析构时输出日志
        ~LogStream() {
            // 检查日志级别
            if(level_ < logger_.logLevel_) {
                return;
            }

            // 添加换行符
            ss_ << "\n";

            // 加锁
            std::lock_guard<std::mutex> lock(logger_.mutex_);

            // 输出日志
            std::cout << ss_.str();

            // 刷新缓冲区
            std::cout.flush();

            // 如果是FATAL日志，终止进程
            if(level_ == LogLevel::FATAL) {
                // 先刷新所有输出流
                std::fflush(stdout);
                std::fflush(stderr);

                std::exit(EXIT_FAILURE);  // 退出
            }
        }

        // 流式输出操作符
        // 重载
        template<typename T>
        LogStream& operator<<(const T& msg) {
            ss_ << msg;
            return *this;
        }

        // 处理流操作符（如std::endl）
        LogStream& operator<<(std::ostream&(*)(std::ostream&)) { return *this; }

    private:
        // 日志级别转字符串
        const char* LogLevelToString(LogLevel level) {
            switch(level) {
                case LogLevel::INFO: {
                    return "[INFO ]";
                }

                case LogLevel::DEBUG: {
                    return "[DEBUG]";
                }

                case LogLevel::WARN: {
                    return "[WARN ]";
                }
                case LogLevel::ERROR: {
                    return "[ERROR]";
                }
                case LogLevel::FATAL: {
                    return "[FATAL]";
                }
                default: {
                    return "[UNKNOWN]";
                }
            }
        }

    private:
        std::ostringstream ss_;     // 字节流
        Logger& logger_;            // 日志对象
        LogLevel level_;            // 日志等级
    };

    // 空日志类
    class NullLogStream
    {
    public:
        template<typename T>
        NullLogStream& operator<<(const T&) {
            return *this;
        }

        // 处理流操作符
        NullLogStream& operator<<(std::ostream&(*)(std::ostream&)) {
            return *this;
        }
    };

private:
    Logger() 
        : logLevel_(LogLevel::INFO)           // 默认INFO级别
    {}

    // 当多个线程同时写入日志时，如果不加锁，
    // 它们的输出可能会相互交织，导致日志混乱
    std::mutex mutex_;                        // 输出互斥锁

    LogLevel logLevel_;                       // 当前日志级别
};

// 日志宏定义
#if LOG_ENABLED
    #define LOG_INFO()  Logger::LogStream(Logger::instance(), LogLevel::INFO , __FILE__, __LINE__)
    #define LOG_DEBUG() Logger::LogStream(Logger::instance(), LogLevel::DEBUG, __FILE__, __LINE__)
    #define LOG_WARN()  Logger::LogStream(Logger::instance(), LogLevel::WARN , __FILE__, __LINE__)
    #define LOG_ERROR() Logger::LogStream(Logger::instance(), LogLevel::ERROR, __FILE__, __LINE__)
    #define LOG_FATAL() Logger::LogStream(Logger::instance(), LogLevel::FATAL, __FILE__, __LINE__)
#else
    #define LOG_DEBUG() while(false) Logger::NullLogStream()
    #define LOG_INFO()  while(false) Logger::NullLogStream()
    #define LOG_WARN()  while(false) Logger::NullLogStream()
    #define LOG_ERROR() while(false) Logger::NullLogStream()
    #define LOG_FATAL() while(false) Logger::NullLogStream()
#endif

#endif