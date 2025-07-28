# ThreadPool
## C++双模式线程池
### 项目结构梳理
```
ThreadPoolV2/
├── autobuild.sh                 # 构建脚本
├── CMakeLists.txt               # 总体 CMake 构建文件
├── .vscode/settings.json        # VSCode 配置文件
│
├── Optimize/                    # 优化版本（std::packaged_task + std::future）
│   ├── include/
│   │   ├── threadOpt.h
│   │   └── threadpoolOpt.h
│   ├── src/
│   │   ├── main.cpp
│   │   └── CMakeLists.txt
│
├── Origin/                      # 原始版本（自定义 Any/Result/Sem 实现）
│   ├── include/
│   │   ├── any.h
│   │   ├── result.h
│   │   ├── sem.h
│   │   ├── task.h
│   │   ├── thread.h
│   │   └── threadpool.h
│   ├── src/
│   │   ├── main.cpp
│   │   ├── result.cpp
│   │   ├── sem.cpp
│   │   ├── task.cpp
│   │   └── CMakeLists.txt
```
### 项目描述
&emsp;&emsp;实现`Fixed/Cached`双模式线程池，支持任务调度、资源动态管理及异步结果获取。`Fixed`模式：固定线程数，低开销；`Cached`模式：动态扩容（上限`1024`线程），`60s`空闲线程自动回收。
### 开发环境
&emsp;&emsp;`Visual Studio Code + WSL2（Ubuntu-22.04）、CMake`
### 项目内容
- 底层机制实现
    - 实现`Any`通用容器，利用类型擦除技术存储任务返回值；
    - 基于信号量同步机制实现`Result`类，支持任务结果异步获取；
    - 继承任务基类`Task`并重写`run`方法，通过`Result`对象异步获取任务结果；
    - 使用中间类`Impl`，解耦任务与结果对象，避免悬垂指针问题。
- 标准库优化重构
    - 使用`std::packaged_task + std::future`替代自定义类型，消除继承约束；
    - 基于可变参模板+引用折叠，重构任务提交接口（`submitTask`），支持任意可调用对象；
    - 通过完美转发实现零拷贝参数传递。