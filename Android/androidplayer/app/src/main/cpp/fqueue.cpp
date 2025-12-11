#include "fqueue.h"

FrameQueue::FrameQueue() {
    abort_request = false;
}

FrameQueue::~FrameQueue() {
    clear();
}

void FrameQueue::push(AVFrame *pkt) {
    std::unique_lock<std::mutex> lock(mtx);

    if (abort_request)
        return;

    queue.push(pkt);
    cv.notify_one();
}

AVFrame *FrameQueue::pop() {
    std::unique_lock<std::mutex> lock(mtx);

    cv.wait(lock, [this]() { return !queue.empty() || abort_request; });

    if (abort_request)
        return nullptr;

    AVFrame *pkt = queue.front();
    queue.pop();

    return pkt;
}

void FrameQueue::clear() {
    std::unique_lock<std::mutex> lock(mtx);

    while (!queue.empty()) {
        AVFrame *pkt = queue.front();
        av_frame_free(&pkt);
        queue.pop();
    }
}

void FrameQueue::abort() {
    std::unique_lock<std::mutex> lock(mtx);

    abort_request = true;
    cv.notify_all();
}

bool FrameQueue::isEmpty() {
    std::unique_lock<std::mutex> lock(mtx);

    return queue.empty();
}