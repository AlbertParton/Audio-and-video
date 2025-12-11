#ifndef AVFRAMEQUEUE_H
#define AVFRAMEQUEUE_H
#include "queue.h"
#ifdef __cplusplus
extern "C" { 
#include "libavcodec/avcodec.h"

}
#endif

class AVFrameQueue
{
public:
    AVFrameQueue();
    ~AVFrameQueue();

    void Abort();
    int Size();
    int Push(AVFrame *val);
    AVFrame *Pop(const int timeout);
    AVFrame *Front();

private:
    void release();// 释放队列中所有 AVFrame 资源（内部使用）
    Queue<AVFrame *> queue_;// 底层线程安全队列，存储 AVFrame 指针
};

#endif // AVFRAMEQUEUE_H
