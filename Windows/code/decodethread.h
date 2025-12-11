#ifndef DECODETHREAD_H
#define DECODETHREAD_H

#include "thread.h"
#include "avpacketqueue.h"
#include "avframequeue.h"

class MainController; // 前向声明

/**
 * @brief 解码线程（DecodeThread）
 *
 * 负责：
 *   1. 从 AVPacketQueue 获取压缩数据包 AVPacket
 *   2. 调用 FFmpeg 解码为 AVFrame
 *   3. 将解码后的帧压入 AVFrameQueue
 *
 * 支持功能：
 *   - 视频/音频统一解码流程
 *   - 暂停与恢复（与 MainController 协作）
 */
class DecodeThread : public Thread
{
public:
    DecodeThread(AVPacketQueue* packet_queue,
        AVFrameQueue* frame_queue,
        MainController* controller);
    ~DecodeThread();

    int Init(AVCodecParameters* par);   // 初始化解码器
    int Start();                         // 启动解码线程
    int Stop();                          // 停止线程
    void Run() override;                 // 线程主循环

    void Flush();                        // Flush 解码器缓冲区

    AVCodecContext* GetAVCodecContext(); // 获取 FFmpeg 解码上下文

private:
    char err2str[256] = { 0 };            // 错误信息字符串缓冲
    AVCodecContext* codec_ctx_ = nullptr; // FFmpeg 解码器上下文

    // ===== 队列 =====
    AVPacketQueue* packet_queue_ = nullptr; // 输入数据包队列
    AVFrameQueue* frame_queue_ = nullptr;   // 解码输出帧队列

    MainController* controller_ = nullptr;  // 主控制器，用于暂停/恢复判断
};

#endif // DECODETHREAD_H
