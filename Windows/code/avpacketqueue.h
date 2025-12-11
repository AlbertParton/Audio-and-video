#ifndef AVPACKETQUEUE_H
#define AVPACKETQUEUE_H
#include "queue.h"
#ifdef __cplusplus
extern "C" {
#include "libavcodec/avcodec.h"

}
#endif
class AVPacketQueue
{
public:
    AVPacketQueue();
    ~AVPacketQueue();

    void Abort();
    int Size();
    int Push(AVPacket *val);
    AVPacket *Pop(const int timeout);

private:
    void release();// 释放队列中所有 AVPacket 资源（内部使用）
    Queue<AVPacket *> queue_;// 底层线程安全队列，存储 AVPacket 指针
};

#endif // AVPACKETQUEUE_H
