# 设置cmake最低版本
cmake_minimum_required(VERSION 3.10)

# 设置工程
project(THREADPOLLOPT LANGUAGES CXX)

# 配置编译器选项
# set(CMAKE_CXX_FLAGS "${CMAKE_FXX_FLAGS} -g")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3")

# 设置头文件搜索路径
include_directories(${PROJECT_SOURCE_DIR}/Origin/include)
include_directories(${PROJECT_SOURCE_DIR}/Optimize/include)
include_directories(${PROJECT_SOURCE_DIR}/tools)

# 设置可执行文件生成路径
set(EXECUTABLE_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/bin)

# 搜索子目录
add_subdirectory(${PROJECT_SOURCE_DIR}/Origin)
add_subdirectory(${PROJECT_SOURCE_DIR}/Optimize)