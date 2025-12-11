#ifndef DECODER_H
#define DECODER_H

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
}

#include <thread>
#include <atomic>
#include "queue.h"
#include "fqueue.h"

class Decoder {
public:
    Decoder();

    ~Decoder();

    bool init(AVCodecParameters *codecpar); // 初始化解码器
    void start();                           // 启动解码线程
    void stop();                            // 停止线程
    void setQueue(PacketQueue *queue);      // 设置输入队列
    void setOutputFile(const char *path);   // 设置输出yuv文件路径
    void setFrameQueue(FrameQueue * queue);        // 设置输出帧队列
    bool isFinished() const;

    int getWidth() const { return width; }
    int getHeight() const { return height; }

private:
    void decodeThread();                    // 解码线程函数

    AVCodecContext *codec_ctx = nullptr;//解码器上下文
    PacketQueue *video_queue = nullptr;//解复用后的 AVPacket 数据包的队列
    FrameQueue *frame_queue = nullptr;//解码后的 AVFrame 数据包的队列
    std::thread worker;
    std::atomic<bool> running;
    FILE *yuv_out = nullptr;//指向输出的 YUV 文件
    int width = 0;
    int height = 0;

};

#endif // DECODER_H
