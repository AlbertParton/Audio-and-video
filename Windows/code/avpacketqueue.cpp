#include "avpacketqueue.h"

/**
 * @brief 构造函数，初始化AVPacketQueue对象
 */
AVPacketQueue::AVPacketQueue() {};

/**
 * @brief 析构函数，释放队列中的资源
 */
AVPacketQueue::~AVPacketQueue()
{
    Abort();
};

/**
 * @brief 终止队列并释放所有资源
 * 此方法会清空队列中的所有数据包并释放资源，然后终止队列
 */
void AVPacketQueue::Abort()
{
    // 先释放队列里的资源
    release();
    // 终止内部队列，唤醒所有等待的线程
    queue_.Abort();
};

/**
 * @brief 获取队列中当前的数据包数量
 * @return 队列中的数据包数量
 */
int AVPacketQueue::Size()
{
    return queue_.Size();
};

/**
 * @brief 将一个AVPacket放入队列
 * @param val 要放入队列的AVPacket指针
 * @return 成功返回0，失败返回负值
 *
 * 注意：此函数会复制数据包的引用（不是完整拷贝数据），
 * 原始数据包的引用计数会被重置为0，意味着调用方不再拥有该数据包
 */
int AVPacketQueue::Push(AVPacket* val)
{
    // 分配一个新的AVPacket
    AVPacket* tmp_pkt = av_packet_alloc();
    // 移动引用，将val的内容移动到tmp_pkt，val的引用计数会被重置为0
    av_packet_move_ref(tmp_pkt, val);
    // 将新数据包放入队列
    return queue_.Push(tmp_pkt);
};

/**
 * @brief 从队列中弹出一个AVPacket
 * @param timeout 等待超时时间，单位为毫秒，0表示不等待
 * @return 成功返回AVPacket指针，失败返回NULL
 *
 * 调用方负责释放返回的AVPacket
 */
AVPacket* AVPacketQueue::Pop(const int timeout)
{
    AVPacket* tmp_pkt = NULL;
    // 从队列中获取一个数据包
    int ret = queue_.Pop(tmp_pkt, timeout);
    if (ret < 0) {
        if (ret == -1) {
            printf("queue_ abort\n ");
        }
        // 队列已终止或出错，返回NULL
        return NULL;
    }
    // 返回队列中的数据包
    return tmp_pkt;
};

/**
 * @brief 释放队列中的所有AVPacket资源
 * 私有方法，用于在Abort或析构时释放所有资源
 */
void AVPacketQueue::release()
{
    while (true) {
        AVPacket* tmp_pkt = NULL;
        // 尝试从队列中获取一个数据包，等待1ms
        int ret = queue_.Pop(tmp_pkt, 1);
        if (ret < 0) {
            // 队列为空或已终止，退出循环
            break;
        }
        else {
            // 释放数据包资源
            av_packet_free(&tmp_pkt);
            continue;
        }
    }
};
