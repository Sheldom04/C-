#include <memory>
#include <iostream>

class B;
class A
{
public:
    std::shared_ptr<B> ptrB;
    ~A(){
        std::cout << "A销毁" << std::endl;
    }
};
class B
{
public:
    std::weak_ptr<A> ptrA;
    ~B()
    {
        std::cout << "B销毁" << std::endl;
    }
};

int main()
{
    std::shared_ptr<A> a = std::make_shared<A>();
    std::shared_ptr<B> b = std::make_shared<B>();
    std::cout << a.use_count() << std::endl;
    std::cout << b.use_count() << std::endl;
    a->ptrB = b;
    b->ptrA = a;  //weak_ptr 基于shared_ptr  防止循环引用问题
    //可以在不增加 shared_ptr计数的情况下 使用 
    if(auto a_ = b->ptrA.lock())  //lock返回shared_ptr
    {
        a_->ptrB;
    }
    else{

    }
    std::cout << a.use_count() << std::endl;
    std::cout << b.use_count() << std::endl;
    return 0;
}