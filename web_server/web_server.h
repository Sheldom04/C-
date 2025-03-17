#pragma once
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/fcntl.h>
#include <sys/epoll.h>
#include <assert.h>
#include <cstring>

#include "fiber/fiber.h"
#include "thread_pool/thread_pool.h"
#include "timer/timer.h"

#define MAX_EVENTS 256 // 监听数量上限

struct fdcontext
{
    int fd;
    int events;
    int len;
    std::shared_ptr<Fiber> fiber;
    std::function<void()> cb;
    int status; // 是否被选取
    std::mutex mutex;
    void reset();
};
class Webserver : public Thread_pool, public TimerManager
{
public:
    enum Event
    {
        // 无事件
        NONE = 0x0,
        // READ == EPOLLIN
        READ = 0x1,
        // WRITE == EPOLLOUT
        WRITE = 0x4
    };
    Webserver(int port, int threads);
    ~Webserver();
    void setnonblock(int fd);
    void addfd(int epfd, int events, int fd);
    void addfd(int epfd, int events, fdcontext *ev);
    void setfdcontext(fdcontext *ev, int fd, std::function<void()> cb);
    void eventLoop(); // 主线程工作事件
    std::vector<fdcontext *> fdcont_vec;
    fdcontext *lfdcont;
    void eventdel(fdcontext *ev);

private:
    void acceptevent();
    // void readevent(int fd);
    void readevent(fdcontext *(&ev));
    void sendevent(fdcontext *&ev);

private:
    int threads;
    int m_lfd;
    int m_port;
    int m_epfd;
    std::shared_mutex m_mutex;
    int num = 0;
    std::mutex epoll_mutex;
    void contextResize(size_t size);

public:
    void idle() override;
    void onTimerInsertedAtFront() override;
    void tickle() {};
};