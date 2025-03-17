#include "timer/timer.h"

Timer::Timer(uint64_t ms, std::function<void()> cb, bool recurring, TimerManager *manager) : m_recurring(recurring), m_ms(ms), m_cb(cb), m_manager(manager)
{
    auto now = std::chrono::system_clock::now();
    //当前时间加上超时时间获得 下一次超时的绝对时间 
    m_nest = now + std::chrono::milliseconds(m_ms);
}
bool Timer::cancel()  //将当前Timer从堆上删除 
{
    std::unique_lock<std::shared_mutex> write_lock(m_manager->m_mutex);
    if (m_cb == nullptr)
    {
        return false;
    }
    m_cb = nullptr;

    auto it = m_manager->m_timers.find(shared_from_this());
    if (it != m_manager->m_timers.end())
    {
        m_manager->m_timers.erase(it);  //Timer智能指针引用计数减1
    }
    return true;
}

bool Timer::refresh()
{
    std::unique_lock<std::shared_mutex> write_lock(m_manager->m_mutex);
    if (!m_cb)
    {
        return false;
    }
    auto it = m_manager->m_timers.find(shared_from_this());
    if (it == m_manager->m_timers.end())  
    {
        return false;  //表示 没找到 已经删除
    }
    m_manager->m_timers.erase(it);
    //重置超时时间再插回去
    auto now = std::chrono::system_clock::now();
    m_nest = now + std::chrono::milliseconds(m_ms);
    m_manager->m_timers.insert(shared_from_this());
    return true;
}

bool Timer::reset(uint64_t ms, bool from_now)
{
    //from_now：true从现在开始计算
    //false从创建定时器开始计算 
    if(ms = m_ms && !from_now)
    {
        true;
    }
    m_ms = ms;
    //在时间堆中找到当前定时器 并删除 
    {
        std::unique_lock<std::shared_mutex> write_lock(m_manager->m_mutex);
        auto it = m_manager->m_timers.find(shared_from_this());
        if (it == m_manager->m_timers.end())
        {
            return false;
        }
        m_manager->m_timers.erase(it);
    }
    //在获取 开始计算时间后再 赋值 m_ms
    auto start = from_now ? std::chrono::system_clock::now() : m_nest - std::chrono::milliseconds(m_ms);
    m_ms = ms;
    m_nest = start + std::chrono::milliseconds(m_ms);
    m_manager->addTimer(shared_from_this());
    return true;
}

bool Timer::Comparator::operator()(const std::shared_ptr<Timer> &lhs, const std::shared_ptr<Timer> &rhs) const  //引用传递 
{
    assert(lhs != nullptr && rhs != nullptr);
    return lhs->m_nest < rhs->m_nest;  //从小到大排序 
}

TimerManager::TimerManager()
{
    m_previouseTime = std::chrono::system_clock::now(); //记录创建时间
}

TimerManager::~TimerManager()
{
}

std::shared_ptr<Timer> TimerManager::addTimer(uint64_t ms, std::function<void()> cb, bool recurring)
{
    //私有构造函数不能用 make_shared
    std::shared_ptr<Timer> timer(new Timer(ms, cb, recurring, this));
    addTimer(timer);
    return timer;
}

//条件存在->执行cb
static void OnTimer(std::weak_ptr<void> weak_cond, std::function<void()> cb)
{
    std::shared_ptr<void> tmp = weak_cond.lock();
    if(tmp)
    {
        cb();
    }
}

std::shared_ptr<Timer> TimerManager::addconditionTimer(uint64_t ms, std::function<void()> cb, std::weak_ptr<void> weak_cond, bool recurring)
{
    //只有条件成立才会执行 cb
    return addTimer(ms, std::bind(OnTimer, weak_cond, cb), recurring);
}

uint64_t TimerManager::getNextTimer()//获得下一个超时事件还剩多少时间 
{
    std::shared_lock<std::shared_mutex> read_lock(m_mutex);  //读锁
    m_tickled = false;

    if(m_timers.empty())
    {
        return ~0ull;  //unsigned long long 取反 
    }
    auto now = std::chrono::system_clock::now();
    auto time = (*m_timers.begin())->m_nest;  //最先超时的Timer
    if(now >= time)
    {
        //有timer超时 但还没删掉 
        return 0;
    }
    else
    {
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(time - now);
        return static_cast<uint64_t>(duration.count());
    }
}

void TimerManager::listExpiredCb(std::vector<std::function<void()>> &cbs)
{
    auto now = std::chrono::system_clock::now();

    std::unique_lock<std::shared_mutex> write_lock(m_mutex);

    bool rollover = detectClockRollover();

    //回退 全部删除  || 超时的删除 
    while(!m_timers.empty() && rollover || !m_timers.empty() && (*m_timers.begin()) ->m_nest <= now)
    {
        std::shared_ptr<Timer> temp = *m_timers.begin();
        m_timers.erase(m_timers.begin());
        cbs.push_back(temp->m_cb);
        if(temp->m_recurring)
        {
            temp->m_nest = now + std::chrono::milliseconds(temp->m_ms);
            m_timers.insert(temp);
        }
        else
        {
            temp->m_cb = nullptr;
        }
    }
}

bool TimerManager::hasTimer()
{
    std::shared_lock<std::shared_mutex> read_lock(m_mutex);
    return !m_timers.empty();
}

void TimerManager::addTimer(std::shared_ptr<Timer> timer)
{
    bool at_front = false;
    {
        std::unique_lock<std::shared_mutex> write_lock(m_mutex);
        auto it = m_timers.insert(timer).first;  //返回 一个pair first 是迭代器 second判断是否成功插入
        at_front = (it == m_timers.begin()) && !m_tickled;
        if (at_front)
        {
            m_tickled = true;
        }
    }
    if (at_front)
    {
        // wake up
        onTimerInsertedAtFront();
    }
}

bool TimerManager::detectClockRollover()
{
    bool rollover = false;
    auto now = std::chrono::system_clock::now();
    //判断在 程序运行过程中时间 是否被回拨
    if(now < (m_previouseTime - std::chrono::milliseconds(60 * 60 * 1000)))
    {
        rollover = true;
    }
    m_previouseTime = now;
    return rollover;
}
