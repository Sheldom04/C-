#include <lock.h>
#include <vector>
#include<iostream>
#include<unistd.h>
int count = 0;
mutex_lock_t mutex_lock;
void add()
{
    while(count < 1000000)  //互斥锁 适用临界区访问时间较长 或存在阻塞
    {
        mutex_lock.lock();
        count++;
        sleep(0.5);
        mutex_lock.unlock();
    }
}

int main()
{
    std::vector<std::thread> thread_vec(8);
    for (int i = 0; i < 8; ++i)
    {
        thread_vec.emplace_back(add);
    }
    while(1)
    {
        std::cout << count << std::endl;
        sleep(1);
    }
    for (int i = 0; i < thread_vec.size(); ++i)
    {
        thread_vec[i].join();
    }
    return 0;
}
