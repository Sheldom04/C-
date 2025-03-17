
#include "thread_pool.h"

Thread_pool::Thread_pool(int threads, int max_thread_num) : live_thread_num(threads), max_thread_num(max_thread_num)
{
    min_thread_num = threads;
    busy_thread_num = 0;
    wait_exit_thread_num = 0;
    shut_down = false;
}

void Thread_pool::start()
{
    std::lock_guard<std::mutex> lock(pool_mxt);
    if (shut_down)
    {
        std::cerr << "Thread_pool is stopped" << std::endl;
        return;
    }

    // 判断是否启动过 防止重复启动
    assert(thread_vec.empty());
    thread_vec.resize(live_thread_num);
    for (int i = 0; i < live_thread_num; i++)
    {
        thread_vec[i] = std::make_shared<std::thread>(std::bind(&Thread_pool::run, this));
        std::cout << "创建工作线程id:" << thread_vec[i]->get_id() << std::endl;
    }
}

/*
    1.根据空闲线程比例去决定线程池的扩容与瘦身
        1)忙线程比例小于0.2时进行瘦身 瘦身至原来的1/2
        2)忙线程比例大于0.8时进行扩容 扩容至原来的2倍
*/
// void Thread_pool::adjust_vecsize()
// {

//     std::shared_lock<std::shared_mutex> write_lock(shared_lock);  //忙线程锁
//     if(busy_thread_num / live_thread_num <= 0.2)
//     {
//         live_thread_num = live_thread_num * 2;
//         thread_vec.resize(live_thread_num, std::make_shared<std::thread>(std::bind(&Thread_pool::run, this)));
//     }
//     else if (busy_thread_num / live_thread_num >= 0.8)
//     {

//     }
//     else
//     {
//         std::this_thread::sleep_for(std::chrono::milliseconds(500)); // 避免忙轮询
//     }
// }

void Thread_pool::add_task(std::function<void()> cb)
{
    assert(cb != nullptr);
    std::lock_guard<std::mutex> lock(pool_mxt); // 不能手动解锁
    Task task(cb);
    task_vec.push_back(task);
}
void Thread_pool::run()
{
    Fiber::GetThis(); // 创建主协程
    Task task;
    std::shared_ptr<Fiber> idle_fiber = std::make_shared<Fiber>(std::bind(&Thread_pool::idle, this));
    // std::cout << "线程id：" << std::this_thread::get_id() << " 创建空闲协程id:" << idle_fiber->getid() << std::endl;
    while (true)
    {
        task.reset();
        {                                               // 获取task
            std::lock_guard<std::mutex> lock(pool_mxt); // 上锁原因 各个线程对task_vec进行读写操作
            if (shut_down && task_vec.empty())
            {
                break;
            }
            auto it = task_vec.begin();
            if (!task_vec.empty())
            {
                task = *it;
                assert(task.fiber || task.m_cb);
                task_vec.erase(it);
            }
        }
        if (task.fiber)
        {
            // {
            //     std::lock_guard<std::mutex> lock(task->fiber->f_mutex);  //协程锁 上锁原因
            if (task.fiber->getState() != Fiber::TERM)
            {
                task.fiber->resume();
            }
            // }
            task.reset();
        }
        else if (task.m_cb)
        {
            std::shared_ptr<Fiber> cb_fiber = std::make_shared<Fiber>(task.m_cb);
            {
                std::lock_guard<std::mutex> lock(cb_fiber->f_mutex);
                // 执行MainFunc执行完自动退出
                cb_fiber->resume();
            }

            task.reset();
        }
        else
        {
            idle_fiber->resume(); // 默认运行空闲协程
        }
    }
}

void Thread_pool::thread_pool_destroy()
{
    {
        std::lock_guard<std::mutex> lock(pool_mxt); // 用完锁要及时解锁
        shut_down = true;
        pool_cv.notify_all();
    }
    for (auto &th : thread_vec)
    {
        if (th && th->joinable())
        {
            th->join();
        }
    }
    if (adjust_tid->joinable())
    {
        adjust_tid->join();
    }
}

void Thread_pool::idle()
{
    while (1)
    {
        std::unique_lock<std::mutex> lock(pool_mxt);
        pool_cv.wait(lock);
        pool_mxt.unlock();
        if (DEBUG)
            std::cout << "条件变量满足" << std::endl;
        Fiber::GetThis()->yield();
    }
}

Thread_pool::~Thread_pool()
{
    thread_pool_destroy();
}
