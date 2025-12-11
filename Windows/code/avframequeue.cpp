#include "avframequeue.h"

/**
 * @brief 构造函数，初始化AVFrameQueue对象
 */
AVFrameQueue::AVFrameQueue() {};

/**
 * @brief 析构函数，释放队列中的资源
 */
AVFrameQueue::~AVFrameQueue()
{
    Abort();
};

/**
 * @brief 终止队列并释放所有资源
 * 此方法会清空队列中的所有帧并释放资源，然后终止队列
 */
void AVFrameQueue::Abort()
{
    // 先释放队列中的所有帧资源
    release();
    // 终止内部队列，唤醒所有等待的线程
    queue_.Abort();
};

/**
 * @brief 获取队列中当前的帧数量
 * @return 队列中的帧数量
 */
int AVFrameQueue::Size()
{
    return queue_.Size();
};

/**
 * @brief 将一个AVFrame放入队列
 * @param val 要放入队列的AVFrame指针
 * @return 成功返回0，失败返回负值
 *
 * 注意：此函数会复制帧的引用（不是完整拷贝帧数据），
 * 原始帧的引用计数会被重置为0，意味着调用方不再拥有该帧
 */
int AVFrameQueue::Push(AVFrame* val)
{
    // 分配一个新的AVFrame
    AVFrame* tmp_frame = av_frame_alloc();
    // 移动引用，将val的内容移动到tmp_frame，val的引用计数会被重置为0
    av_frame_move_ref(tmp_frame, val);
    // 将新帧放入队列
    return queue_.Push(tmp_frame);
};

/**
 * @brief 从队列中弹出一个AVFrame
 * @param timeout 等待超时时间，单位为毫秒，0表示不等待
 * @return 成功返回AVFrame指针，失败返回NULL
 *
 * 调用方负责释放返回的AVFrame
 */
AVFrame* AVFrameQueue::Pop(const int timeout)
{
    AVFrame* tmp_frame = NULL;
    // 从队列中获取一个帧
    int ret = queue_.Pop(tmp_frame, timeout);
    if (ret < 0) {
        if (ret == -1) {
            printf("queue_ abort\n ");
        }
        // 队列已终止或出错，返回NULL
        return NULL;
    }
    // 返回队列中的帧
    return tmp_frame;
};

/**
 * @brief 查看队列中的第一个AVFrame，但不移除它
 * @return 成功返回AVFrame指针，失败返回NULL
 *
 * 注意：返回的是帧的引用，不要释放这个指针
 */
AVFrame* AVFrameQueue::Front()
{
    AVFrame* tmp_frame = NULL;
    // 获取队列首部的帧但不移除
    int ret = queue_.Front(tmp_frame);
    if (ret < 0) {
        if (ret == -1) {
            printf("queue_ abort\n ");
        }
        // 队列已终止或出错，返回NULL
        return NULL;
    }
    // 返回队列中的帧
    return tmp_frame;
};

/**
 * @brief 释放队列中的所有AVFrame资源
 * 私有方法，用于在Abort或析构时释放所有资源
 */
void AVFrameQueue::release()
{
    while (true) {
        AVFrame* tmp_frame = NULL;
        // 尝试从队列中获取一个帧，等待1ms
        int ret = queue_.Pop(tmp_frame, 1);
        if (ret < 0) {
            // 队列为空或已终止，退出循环
            break;
        }
        else {
            // 释放帧资源
            av_frame_free(&tmp_frame);
            continue;
        }
    }
};
