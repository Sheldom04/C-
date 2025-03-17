cmake_minimum_required(VERSION 3.10)
project(MyServer)

set(CMAKE_CXX_STANDARD 17)  # 使用 C++11
set(CMAKE_CXX_FLAGS "-pthread")  # 链接 pthread
set(CMAKE_CXX_FLAGS "-g")  # 链接 pthread

# 设置头文件搜索路径
include_directories(${PROJECT_SOURCE_DIR})

# 添加源文件
set(SRC_FILES
    web_server/web_server.cpp
    thread_pool/thread_pool.cpp
    fiber/fiber.cpp
    timer/timer.cpp
    main.cpp
)

# 生成可执行文件 (避免使用 "test")
add_executable(server_app ${SRC_FILES})

# 链接 pthread
target_link_libraries(server_app pthread)
