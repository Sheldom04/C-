#ifndef FIBER_H
#define FIBER_H

#include <ucontext.h> //用于保存协程上下文
#include <memory>     // 包含 std::enable_shared_from_this 和 std::shared_ptr
#include <assert.h>
#include <atomic>
#include <functional>
#include <mutex>
#include <iostream>
class Fiber : public std::enable_shared_from_this<Fiber>
{
public:
    enum State
    {
        READY,
        RUNNING,
        TERM
    };

private:
    Fiber();

public:
    // static 允许在没有实例的时候直接调用
    static std::shared_ptr<Fiber> GetThis();
    static void MainFunc();
    void SetThis(Fiber *f);
    Fiber(std::function<void()> cb, size_t stacksize = 0, bool run_in_scheduler = true);
    ~Fiber();
    void yield(); // 利用上下文协程上下文交换 来启动 会退出
    void resume();
    State getState() const { return m_state; }
    uint64_t getid() const { return m_id; }

private : State m_state;
    ucontext_t m_ctx;
    uint64_t m_id;              // 协程id
    std::function<void()> m_cb; // 协程任务函数
    bool m_runInScheduler;      // 是否让出执行权给调度协程
    size_t m_stacksize;         // 协程占用栈空间大小
    void *m_stack;              // 协程的栈
public:
    std::mutex f_mutex;
};
#endif