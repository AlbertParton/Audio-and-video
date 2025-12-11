#include "thread.h"
#include <stdio.h>

/**
 * @brief 构造函数
 */
Thread::Thread() {};

/**
 * @brief 析构函数，释放线程资源
 *
 * 基类析构函数为虚函数，确保派生类对象正确析构
 */
Thread::~Thread() {};

/**
 * @brief 启动线程
 * @return 成功返回0
 *
 * 此为基类的默认实现，派生类应重写此方法
 */
int Thread::Start()
{
    return 0;
};

/**
 * @brief 停止线程
 * @return 成功返回0
 *
 * 设置终止标志并等待线程结束，释放线程资源
 */
int Thread::Stop()
{
    abort_ = 1;

    // 如果线程存在，等待线程结束并释放资源
    if (thread_) {
        thread_->join();  // 等待线程结束
        delete thread_;   // 释放线程对象
        thread_ = nullptr;
    }

    return 0;
};
