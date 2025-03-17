#include "fiber.h"

// 设置一些协程信息 thread_local  每个线程都会创建一个副本
// 正在运行协程 即每个线程都有 主协程 调度协程 运行协程
static thread_local Fiber *t_fiber = nullptr;
// 主协程
static thread_local std::shared_ptr<Fiber> t_thread_fiber = nullptr;
// 调度协程
static thread_local Fiber *t_scheduler_fiber = nullptr;

// 所有线程共享 s是shared的意思
static std::atomic<uint64_t> s_fiber_id{0};
static std::atomic<uint64_t> s_fiber_count{0};

// 修改一些需要修改的变脸 保存当前协程上下文
Fiber::Fiber()
{                  // 修改当前正在运行协程 t_fiber；状态 m_state；当前运行协程id m_id ；总协程数+1
    SetThis(this); // 创建主协程后 设置当前正在运行协程
    m_state = RUNNING;
    int ret = getcontext(&m_ctx); // 获得主线程的上下文
    assert(ret == 0);
    m_id = s_fiber_id++;
    s_fiber_count++;
}

// 将传入的协程设置为当前正在运行的协程
void Fiber::SetThis(Fiber *f)
{
    t_fiber = f;
}

// 若已经创建了主协程则获得当前协程 没有则创建主协程并返回
std::shared_ptr<Fiber> Fiber::GetThis()
{
    if (t_fiber)
    {
        return t_fiber->shared_from_this();
    }

    // 私有构造函数不能用 make_shared 只能用 new  这一点很重要！！！
    std::shared_ptr<Fiber> main_fiber(new Fiber());
    t_thread_fiber = main_fiber;
    t_scheduler_fiber = main_fiber.get(); // 除非主动设置 主协程默认为调度协程
    assert(t_fiber == main_fiber.get());
    return t_fiber->shared_from_this();
}

Fiber::Fiber(std::function<void()> cb, size_t stacksize, bool run_in_scheduler) : m_cb(cb), m_runInScheduler(run_in_scheduler)
{
    m_state = READY;
    m_stacksize = stacksize ? stacksize : 1280000;
    m_stack = malloc(m_stacksize); // 在堆上开辟内存空间
    int ret = getcontext(&m_ctx);  // 初始化上下文
    assert(ret == 0);
    m_ctx.uc_link = nullptr;
    m_ctx.uc_stack.ss_sp = m_stack;
    m_ctx.uc_stack.ss_size = m_stacksize;
    makecontext(&m_ctx, &Fiber::MainFunc, 0); // 绑定协程函数

    m_id = s_fiber_id++;
    s_fiber_count++;
}

void Fiber::MainFunc()
{
    // 主要任务 执行与当前协程绑定的回调函数
    // 不会返回主协程 因为本函数只会与子协程绑定
    std::shared_ptr<Fiber> cur_fiber = GetThis();
    assert(cur_fiber != nullptr);

    cur_fiber->m_cb();
    cur_fiber->m_cb = nullptr; // 手动交换 不自动交换
    cur_fiber->m_state = TERM;

    auto raw_ptr = cur_fiber.get();
    cur_fiber.reset();
    raw_ptr->yield();
}

void Fiber::resume()
{
    assert(m_state == READY);
    m_state = RUNNING;
    if (m_runInScheduler) // 表示当前一定是调度协程
    {
        SetThis(this);
        int ret = swapcontext(&(t_scheduler_fiber->m_ctx), &m_ctx);
        assert(ret == 0);
    }
    else
    {
        SetThis(this);
        int ret = swapcontext(&(t_thread_fiber->m_ctx), &m_ctx);
        assert(ret == 0);
    }
}
void Fiber::yield()
{
    assert(m_state == RUNNING || m_state == TERM);
    if (m_state == RUNNING) // 没有执行完 等待下一次调度
    {
        m_state = READY;
    }
    if (m_runInScheduler)
    {
        // 返回调度协程
        SetThis(t_scheduler_fiber);
        int ret = swapcontext(&m_ctx, &(t_scheduler_fiber->m_ctx));
        assert(ret == 0);
    }
    else
    {
        // 返回主协程
        SetThis(t_thread_fiber.get());
        int ret = swapcontext(&m_ctx, &(t_thread_fiber->m_ctx));
        assert(ret == 0);
    }
}

Fiber::~Fiber()
{
    s_fiber_count--;
    if (m_stack)
    {
        free(m_stack);
    }
}
