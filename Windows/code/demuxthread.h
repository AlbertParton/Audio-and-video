#ifndef DEMUXTHREAD_H
#define DEMUXTHREAD_H

#include <atomic>
#include <thread>
#include <mutex>
#include <condition_variable>

#include "avpacketqueue.h"

extern "C" {
#include <libavformat/avformat.h>
}

// 前向声明避免循环依赖
class MainController;

/*
 * DemuxThread
 * 【解复用线程】
 * 功能：
 *   - 打开输入媒体文件（AVFormatContext）
 *   - 循环读取 AVPacket（av_read_frame）
 *   - 根据 stream_index 推入对应队列（音频/视频）
 *   - 支持暂停（与 MainController 协同）
 *
 * 注：
 *   - 本类支持动态切换 PacketQueue（例如切换文件时）
 */
class DemuxThread
{
public:
    DemuxThread(AVPacketQueue* audio_queue, AVPacketQueue* video_queue, MainController* controller);
    DemuxThread();                         // 备用构造
    ~DemuxThread();

    // 初始化输入媒体（打开文件 + 找到音视频流）
    int Init(const char* url);

    // 启动/停止线程
    int Start();
    int Stop();

    // ===== 获取媒体信息 =====
    AVFormatContext* IfmtCtx();            // 获取 AVFormatContext
    int VideoStreamIndex();                // 获取视频 stream index
    int AudioStreamIndex();                // 获取音频 stream index

    AVCodecParameters* AudioCodecParameters();// 获取音频流编解码参数
    AVCodecParameters* VideoCodecParameters();// 获取视频流编解码参数

    AVRational AudioStreamTimebase();// 获取音频流时间基
    AVRational VideoStreamTimebase();// 获取视频流时间基

private:
    void Run();                            // 线程主循环

private:
    std::thread thread_;                   // demux 后台线程
    std::atomic<bool> abort_{ false };     // 退出标志

    AVFormatContext* ifmt_ctx_ = nullptr;  // 输入媒体上下文

    int audio_stream_ = -1;                // 音频流索引
    int video_stream_ = -1;                // 视频流索引

    // 指向外部队列（允许切换）
    AVPacketQueue* audio_queue_ = nullptr;
    AVPacketQueue* video_queue_ = nullptr;
    std::mutex queue_mtx_;                 // 保护队列指针切换

    MainController* controller_ = nullptr; // 控制器，用于暂停

    char err2str_[128];                    // 错误字符串缓存
};

#endif // DEMUXTHREAD_H
