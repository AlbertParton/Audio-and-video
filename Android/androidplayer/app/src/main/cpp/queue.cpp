#include "queue.h"

PacketQueue::PacketQueue() {
    abort_request = false;
}

PacketQueue::~PacketQueue() {
    clear();
}

void PacketQueue::push(AVPacket *pkt) {
    std::unique_lock<std::mutex> lock(mtx);

    if (abort_request)
        return;

    queue.push(pkt);
    cv.notify_one();
}

AVPacket *PacketQueue::pop() {
    std::unique_lock<std::mutex> lock(mtx);

    cv.wait(lock, [this]() { return !queue.empty() || abort_request; });

    if (abort_request)
        return nullptr;

    AVPacket *pkt = queue.front();
    queue.pop();

    return pkt;
}

void PacketQueue::clear() {
    std::unique_lock<std::mutex> lock(mtx);

    while (!queue.empty()) {
        AVPacket *pkt = queue.front();
        av_packet_free(&pkt);
        queue.pop();
    }
}

void PacketQueue::abort() {
    std::unique_lock<std::mutex> lock(mtx);

    abort_request = true;
    cv.notify_all();
}

bool PacketQueue::isEmpty() {
    std::unique_lock<std::mutex> lock(mtx);

    return queue.empty();
}


