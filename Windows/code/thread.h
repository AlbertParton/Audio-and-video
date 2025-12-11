#ifndef THREAD_H
#define THREAD_H

#include <thread>

class Thread
{
public:
    Thread();
    virtual ~Thread();

    virtual int Start();
    virtual int Stop();
    virtual void Run() = 0;
protected:
    int abort_ = 0;// 终止标志：0=运行中，1=已终止
    std::thread *thread_ = nullptr;// 线程对象指针（使用指针以便延迟创建）
};

#endif // THREAD_H
