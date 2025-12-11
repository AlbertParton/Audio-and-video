#ifndef DEMUXER_H
#define DEMUXER_H

extern "C" {
#include <libavformat/avformat.h>
}

#include <thread>
#include <atomic>
#include "queue.h"

class Demuxer {
public:
    Demuxer();

    ~Demuxer();

    bool open(const char *filepath);          // 打开视频文件
    void start();                             // 启动解复用线程
    void stop();                              // 停止线程
    void setQueue(PacketQueue *queue);        // 设置队列对象
    int getVideoStreamIndex() const;          // 获取视频流索引

    AVCodecParameters *getVideoCodecParams(); // 获取视频流参数
    bool isFinished() const;                   // 线程是否运行结束

private:
    void demuxThread();                       // 线程函数

    AVFormatContext *fmt_ctx = nullptr;//存储解复用上下文
    int video_stream_index = -1;//视频流索引
    std::thread worker;
    std::atomic<bool> running;
    PacketQueue *video_queue = nullptr;//保存视频数据包的队列
};

#endif // DEMUXER_H
