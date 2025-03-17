#include "fiber/fiber.h"
#include <iostream>

void func()
{
    std::cout << "正在执行工作协程\n";
}
int main()
{
    // 创建主协程
    Fiber::GetThis();
    std::shared_ptr<Fiber> fiber = std::make_shared<Fiber>(func, 0, true);
    fiber->resume();
    std::cout << "执行结束\n";
    return 0;
}