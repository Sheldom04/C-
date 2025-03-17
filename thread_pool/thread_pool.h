#pragma once

#include <iostream>
#include <mutex>
#include <vector>
#include <condition_variable>
#include <functional>
#include <thread>
#include <sys/syscall.h>
#include <unistd.h>
#include <assert.h>
#include <shared_mutex>

#include "fiber/fiber.h"

#define DEBUG 0

class Thread_pool
{
public:
    // 操作函数
    Thread_pool(int thread_num, int max_thread_num = 100); // 构造函数 做一些变量初始化
    void add_task(std::function<void()> cb);               // 任务以回调函数的形式
    void thread_pool_destroy();
    void start(); //创建线程池
    virtual void idle(); // 空闲协程的工作函数
    virtual ~Thread_pool();

public:
    struct Task
    {
        // 函数加函数的参数
        std::function<void()> m_cb; // void (*function)(void *arg);
        std::shared_ptr<Fiber> fiber;
        Task(std::function<void()> cb)
        {
            m_cb = cb;
        }
        Task()
        {
            m_cb = nullptr;
            fiber = nullptr;
        }
        Task(std::shared_ptr<Fiber> f)
        {
            fiber = f;
        }
        void reset()
        {
            fiber = nullptr;
            m_cb = nullptr;
        }
    };
    void adjust_vecsize();
    void run();

public:
    std::vector<std::shared_ptr<std::thread>> thread_vec;
    std::vector<Task> task_vec;

private:
    // 线程容器  任务容器  锁  条件变量
    std::mutex pool_mxt;
    std::shared_mutex shared_lock;  //用于空闲线程数的读写 
    std::condition_variable pool_cv;
    std::shared_ptr<std::thread> adjust_tid; // 管理线程
    uint16_t min_thread_num;                 // 最小线程数
    uint16_t max_thread_num;                 // 最大线程数
    uint16_t live_thread_num;                // 存活线程数
    uint16_t busy_thread_num;                // 忙线程数
    uint16_t wait_exit_thread_num;
    bool shut_down;
};