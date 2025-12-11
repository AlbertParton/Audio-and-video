#ifndef QUEUE_H
#define QUEUE_H
#include <mutex>
#include <condition_variable>
#include <queue>

/**
 * @brief 线程安全的队列模板类
 * @tparam T 队列元素类型
 *
 * 该队列提供了线程安全的入队、出队操作，支持超时等待和优雅终止
 */
template <typename T>
class Queue
{
public:
    /**
     * @brief 默认构造函数
     */
    Queue() {};

    /**
     * @brief 析构函数
     */
    ~Queue() {};

    /**
     * @brief 终止队列操作
     *
     * 设置终止标志，并通知所有等待的线程，使它们从等待中返回
     * 调用后，所有队列操作将返回错误
     */
    void Abort()
    {
        abort_ = 1;                    // 设置终止标志
        cond_.notify_all();            // 唤醒所有等待的线程
    };

    /**
     * @brief 向队列中添加元素
     * @param val 要添加的元素
     * @return 成功返回0，队列已终止返回-1
     */
    int Push(T val)
    {
        std::lock_guard<std::mutex> lock(mutex_);  // 获取互斥锁

        if (1 == abort_) {              // 检查队列是否已终止
            return -1;
        }

        queue_.push(val);              // 元素入队
        cond_.notify_one();            // 通知一个等待的消费者线程

        return 0;
    };

    /**
     * @brief 从队列中取出元素
     * @param val 用于接收取出元素的引用
     * @param timeout 等待超时时间（毫秒），默认为0（不等待）
     * @return 成功返回0，队列已终止返回-1，超时返回-2
     */
    int Pop(T& val, const int timeout = 0)
    {
        std::unique_lock<std::mutex> lock(mutex_); // 获取互斥锁
        if (queue_.empty()) {           // 如果队列为空
            // 等待push或者超时唤醒
            cond_.wait_for(lock, std::chrono::milliseconds(timeout), [this] {
                // 等待条件：队列非空或队列已终止
                return !queue_.empty() | (abort_ == 1);
                });
        }
        if (1 == abort_) {              // 检查队列是否已终止
            return -1;
        }
        if (queue_.empty()) {           // 检查是否因超时仍为空
            return -2;
        }
        val = queue_.front();          // 获取队首元素
        queue_.pop();                  // 移除队首元素

        return 0;
    };

    /**
     * @brief 获取队首元素（不移除）
     * @param val 用于接收队首元素的引用
     * @return 成功返回0，队列已终止返回-1，队列为空返回-2
     */
    int Front(T& val)
    {
        std::lock_guard<std::mutex> lock(mutex_);  // 获取互斥锁
        if (1 == abort_) {              // 检查队列是否已终止
            return -1;
        }
        if (queue_.empty()) {           // 检查队列是否为空
            return -2;
        }
        val = queue_.front();          // 获取队首元素

        return 0;
    };

    /**
     * @brief 获取队列当前大小
     * @return 队列中的元素数量
     */
    int Size()
    {
        std::lock_guard<std::mutex> lock(mutex_);  // 获取互斥锁
        return queue_.size();          // 返回队列大小
    };

private:
    int abort_ = 0;                    // 终止标志：0-运行中，1-已终止
    std::mutex mutex_;                 // 互斥锁，保护队列操作
    std::condition_variable cond_;     // 条件变量，用于线程间同步
    std::queue<T> queue_;              // 底层队列容器
};

#endif // QUEUE_H