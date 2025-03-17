#include "web_server/web_server.h"
#include "lock/lock.h"
int main()
{
    // 运行构造函数 创建线程池、创建lfd、epfd并将lfd挂上树
    Webserver Webserver(8080, 10); // port  threads
    Webserver.eventLoop();
    return 0;
}