#pragma once

#include <memory>
#include <functional>
#include <shared_mutex>
#include <chrono>
#include <vector>
#include <mutex>
#include <set>
#include <assert.h>

class TimerManager;
class Timer : public std::enable_shared_from_this<Timer>
{
    friend class TimerManager; //声明友元类 
public:
    //一些定时器的添加删除操作
    bool cancel();  //删除timer
    bool refresh(); //刷新timer
    bool reset(uint64_t ms, bool from_now);  //重设timer超时时间
private:
    Timer(uint64_t ms, std::function<void()> cb, bool recurring, TimerManager *manager);  //构造函数私有化 

private:
    bool m_recurring = false;  //是否循环
    uint64_t m_ms = 0; //超时时间
    //记录定时器下一次的触发时间
    std::chrono::time_point<std::chrono::system_clock> m_nest;
    std::function<void()> m_cb;
    TimerManager *m_manager = nullptr;

private:
    //实现最小堆的比较函数
    struct Comparator
    {
        //形参中的const保证传入参数不会改变 
        //函数后的const 保证成员函数不会改变成员变量 
        bool operator()(const std::shared_ptr<Timer> &lhs, const std::shared_ptr<Timer> &rhs) const;
    };
};

class TimerManager
{
    friend class Timer;
public:
    TimerManager();
    virtual ~TimerManager();
    //添加timer
    std::shared_ptr<Timer> addTimer(uint64_t ms, std::function<void()> cb, bool recurring = false);

    //添加条件timer
    std::shared_ptr<Timer> addconditionTimer(uint64_t ms, std::function<void()> cb, std::weak_ptr<void> weak_cond, bool recurring = false);

    uint64_t getNextTimer();

    //取出所有超时定时器的回调函数 
    void listExpiredCb(std::vector<std::function<void()>> &cbs);

    bool hasTimer();
protected:
    //当一个最早的timer加入堆中 调用该函数 
    virtual void onTimerInsertedAtFront() {};  //继承任务重写
    // 添加timer
    void addTimer(std::shared_ptr<Timer> timer);
private:
    bool detectClockRollover();
    std::shared_mutex m_mutex;
    // 时间堆
    // Comparator仿函数比较器 
    std::set<std::shared_ptr<Timer>, Timer::Comparator> m_timers;
    // 在下次getNextTime()执行前 onTimerInsertedAtFront()是否已经被触发了 -> 在此过程中 onTimerInsertedAtFront()只执行一次
    bool m_tickled = false;
    // 上次检查系统时间是否回退的绝对时间
    std::chrono::time_point<std::chrono::system_clock> m_previouseTime;
};