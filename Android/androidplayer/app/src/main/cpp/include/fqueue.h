#ifndef FQUEUE_H
#define FQUEUE_H

extern "C" {
#include <libavcodec/avcodec.h>
}

#include <queue>
#include <mutex>
#include <condition_variable>

class FrameQueue {
public:
    FrameQueue();

    ~FrameQueue();

    void push(AVFrame *pkt);
    AVFrame *pop();
    void clear();
    void abort();
    bool isEmpty();

private:
    std::queue<AVFrame *> queue;
    std::mutex mtx;
    std::condition_variable cv;
    bool abort_request;
};

#endif //FQUEUE_H
