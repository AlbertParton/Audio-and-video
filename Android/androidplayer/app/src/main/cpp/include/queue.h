#ifndef QUEUE_H
#define QUEUE_H

extern "C" {
#include <libavcodec/avcodec.h>
}

#include <queue>
#include <mutex>
#include <condition_variable>

class PacketQueue {
public:
    PacketQueue();

    ~PacketQueue();

    void push(AVPacket *pkt);
    AVPacket *pop();
    void clear();
    void abort();
    bool isEmpty();

private:
    std::queue<AVPacket *> queue;
    std::mutex mtx;
    std::condition_variable cv;
    bool abort_request;
};

#endif // QUEUE_H
