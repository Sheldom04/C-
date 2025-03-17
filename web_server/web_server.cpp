#include "web_server.h"

void Webserver::setnonblock(int fd)
{
    int flag = fcntl(fd, F_GETFL);
    flag |= O_NONBLOCK;
    fcntl(fd, F_SETFL, flag);
}

void Webserver::addfd(int epfd, int events, fdcontext *ev)
{
    assert(epfd > 0);
    epoll_event event = {0, {0}};
    int op;
    event.events = ev->events = events;
    event.data.ptr = ev;
    op = EPOLL_CTL_ADD;
    if (epoll_ctl(m_epfd, op, ev->fd, &event) >= 0)
    {
        // std::cout << "将fd=" << ev->fd << "挂上树" << std::endl;
    }
    else
    {
        std::cout << "fd=" << ev->fd << "事件添加失败" << std::endl;
        perror("epoll_ctl error");
    }
    setnonblock(ev->fd);
}

void Webserver::setfdcontext(fdcontext *ev, int fd, std::function<void()> cb)
{
    ev->fd = fd;                    // 设置文件描述符
    ev->events = EPOLLIN | EPOLLET; // 默认监听读事件
    ev->cb = cb;                    // 设置回调函数
    ev->status = 0;                 // 初始状态为未在监听树上
    ev->fiber = nullptr;            // 事件对应的协程
}

Webserver::Webserver(int port, int threads) : threads(threads), m_port(port), Thread_pool(threads)
{
    // 开始网络编程
    m_lfd = socket(AF_INET, SOCK_STREAM, 0);

    int opt = 1;
    // setsockopt(m_lfd, SOL_SOCKET, SO_REUSEPORT, &opt, sizeof(opt));
    setsockopt(m_lfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    sockaddr_in serv_addr;
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(m_port);
    serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    bind(m_lfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr));
    listen(m_lfd, 100);
    m_epfd = epoll_create(5000); // 参数大于0即可

    lfdcont = new fdcontext();
    setfdcontext(lfdcont, m_lfd, std::bind(&Webserver::acceptevent, this));
    addfd(m_epfd, EPOLLIN, lfdcont);

    contextResize(MAX_EVENTS);
    start();
}

void Webserver::acceptevent()
{
    // lfd对应的任务
    sockaddr_in clit_addr;
    socklen_t clit_len = sizeof(clit_addr);
    int cfd;
    while ((cfd = accept(m_lfd, (struct sockaddr *)&clit_addr, &clit_len)) == -1)
    {
        if (errno == EAGAIN || errno == EINTR)
        {
            // 没有新连接，继续等待
            std::this_thread::sleep_for(std::chrono::milliseconds(10)); // 避免 CPU 空转
            continue;
        }
        perror("accept error");
        break; // 出现其他错误时退出
    }

    fdcontext *fd_ctx = nullptr;

    std::shared_lock<std::shared_mutex> read_lock(m_mutex); // 读写锁 可以多个读 只能一个写
    // 查找可用的fdcontext
    if ((int)fdcont_vec.size() > cfd)
    {
        // 不需要扩容
        fd_ctx = fdcont_vec[cfd];
        read_lock.unlock();
    }
    else
    {
        // 需要扩容
        read_lock.unlock();
        std::unique_lock<std::shared_mutex> write_lock(m_mutex);
        contextResize((int)(cfd * 1.5));
        std::cout << "扩容至" << cfd * 1.5 << std::endl;
        fd_ctx = fdcont_vec[cfd];
    }
    std::lock_guard<std::mutex> lock(fd_ctx->mutex);
    // 设置fdcontext并添加到epoll
    assert(fd_ctx != nullptr);
    setfdcontext(fd_ctx, cfd, std::bind(&Webserver::readevent, this, fd_ctx));
    assert(fd_ctx->cb != nullptr);
    addfd(m_epfd, EPOLLIN | EPOLLET, fd_ctx);
}

void Webserver::readevent(fdcontext *&ev)
{
    int fd = ev->fd;
    char buf[1024];
    memset(buf, 0, sizeof(buf));
    // while (true)
    // {
    int ret = 0;
    int nread;
    while ((nread = recv(fd, buf + ret, BUFSIZ - 1, 0)) > 0)
    {
        ret += nread;
    }
    if (nread == -1 && errno != EAGAIN && errno != EWOULDBLOCK)
    {
        perror("recv error");
    }
    // int ret = recv(fd, buf, sizeof(buf), 0);
    if (ret > 0)
    {
        // 构建HTTP响应
        const char *response = "HTTP/1.1 200 OK\r\n"
                               "Content-Type: text/plain\r\n"
                               "Content-Length: 13\r\n"
                               "Connection: keep-alive\r\n"
                               "\r\n"
                               "Hello, World!";

        // 发送HTTP响应
        ret = send(fd, response, strlen(response), 0);

        // 关闭连接
        eventdel(ev);
        close(fd);
    }
    // if (ret <= 0)
    // {
    //     if (ret == 0 || errno != EAGAIN)
    //     {
    //         eventdel(ev);
    //         close(fd);
    //     }
    //     else if (errno == EAGAIN) // 无数据可读
    //     {
    //     }
    // }
    // }
}

void Webserver::eventLoop()
{
    // 主线程阻塞等待 线程执行完毕
    for (int i = 0; i < thread_vec.size(); i++)
    {
        if (thread_vec[i]->joinable()) // 等待 子线程执行完毕
        {
            thread_vec[i]->join();
        }
    }
}

Webserver::~Webserver()
{
    // 关闭文件描述符
    close(m_epfd);
    close(m_lfd);
    for (size_t i = 0; i < fdcont_vec.size(); ++i)
    {
        if (fdcont_vec[i])
        {
            delete fdcont_vec[i];
        }
    }
}

void Webserver::idle()
{
    static const uint64_t MAX_EVNETS = 32;
    std::unique_ptr<epoll_event[]> events(new epoll_event[MAX_EVNETS]); // 创建events数组 用于 epoll_wait
    while (true)
    {
        // std::cout << "task_vec的大小:" << task_vec.capacity() * sizeof(Task) << std::endl;
        int ret = 0;
        while (true)
        {
            // 设置最大超时时间
            static const uint64_t MAX_TIMEOUT = 5000;
            uint64_t next_timeout = getNextTimer();
            next_timeout = std::min(next_timeout, MAX_TIMEOUT);
            ret = epoll_wait(m_epfd, events.get(), MAX_EVNETS, (int)next_timeout);
            if (ret < 0 && errno == EINTR)
            {
                // 被信号中断
                continue;
            }
            else
            {
                break;
            }
        }

        std::vector<std::function<void()>> cbs;
        listExpiredCb(cbs); // 收集超时事件
        if (!cbs.empty())
        {
            for (const auto &cb : cbs)
            {
                add_task(cb); // 添加超时任务
            }
            cbs.clear();
        }

        // 向任务队列添加任务
        for (int i = 0; i < ret; i++)
        {
            epoll_event &event = events[i];
            fdcontext *ev = (fdcontext *)event.data.ptr;
            std::lock_guard<std::mutex> lock(ev->mutex); // 加锁原因 不同线程的 epoll_wait 可能会返回相同的fd
            if (event.events & (EPOLLERR | EPOLLHUP))
            {
                event.events |= (EPOLLIN | EPOLLOUT) & ev->events;
            }

            if (ev->events & EPOLLIN || EPOLLOUT)
            {
                // 添加任务
                num++;
                assert(ev->cb != nullptr);
                add_task(ev->cb);
            }
            // std::cout << "处理事件: fd=" << ev->fd << ", events=" << event.events << std::endl;
        }
        // std::cout << "num=" << num << std::endl;
        if (num > 0)
        {
            Fiber::GetThis()->yield(); // 仅当有任务时才 yield
        }
    }
}

void fdcontext::reset()
{
    fd = 0;
    events = 0;
    fiber = nullptr;
    cb = nullptr;
    status = 0;
    mutex.unlock();
}

void Webserver::eventdel(fdcontext *ev)
{
    epoll_event epv = {0, {0}};
    epv.data.ptr = NULL;                            // 从树上删除但是文件描述符仍在使用
    epoll_ctl(m_epfd, EPOLL_CTL_DEL, ev->fd, &epv); // 从红黑树 efd 上将 ev->fd 摘除
    return;
}

void Webserver::sendevent(fdcontext *&ev)
{
    const char *response = "HTTP/1.1 200 OK\r\n"
                           "Content-Type: text/plain\r\n"
                           "Content-Length: 13\r\n"
                           "Connection: keep-alive\r\n"
                           "\r\n"
                           "Hello, World!";
    int ret = send(ev->fd, response, strlen(response), 0);
    if (fcntl(ev->fd, F_GETFD) == -1)
    {
        perror("fd 已关闭或无效");
    }
    if (ret > 0)
    {
    }
    else
    {
        std::cout << "发送数据失败" << std::endl;
        perror("send error");
    }
    eventdel(ev);
    close(ev->fd);
    ev->status = 0; // 全部删除后再置0
}

// 当加入的 定时器是绝对超时时间最近时触发
void Webserver::onTimerInsertedAtFront()
{
    tickle();
}

void Webserver::contextResize(size_t size)
{
    fdcont_vec.resize(size);

    for (int i = 0; i < fdcont_vec.size(); i++)
    {
        if (fdcont_vec[i] == nullptr)
        {
            fdcont_vec[i] = new fdcontext();
            // 创建的时候设置好fd的值对应下标 根据fd获得对应ctx
            fdcont_vec[i]->fd = i;
        }
    }
}
