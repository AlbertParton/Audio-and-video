#ifndef AUDIOOUTPUT_H
#define AUDIOOUTPUT_H

#include "avframequeue.h"
#include "avsync.h"

#ifdef __cplusplus
extern "C" {
#include "SDL.h"
#include "libswresample/swresample.h"
#include "libavfilter/avfilter.h"
#include "libavfilter/buffersink.h"
#include "libavfilter/buffersrc.h"
#include "libavutil/opt.h"
#include "libavutil/channel_layout.h"
#include "libavutil/samplefmt.h"
}
#endif

/**
 * @brief 音频参数结构体（包含采样率 / 声道布局 / 采样格式）
 *        此结构由解析器或解码器初始化后传入。
 */
typedef struct _AudioParams {
    int freq;                   // 采样率
    AVChannelLayout ch_layout;  // 声道布局
    enum AVSampleFormat fmt;    // 采样格式
} AudioParams;

/**
 * @brief 音频输出模块（负责音频重采样、ATempo、SDL 播放）
 *
 * 主要功能：
 * - 使用 SDL 播放音频
 * - 构建 FFmpeg ATempo 滤镜图以支持倍速播放
 * - 使用 SwrContext 进行格式转换（如需要）
 * - 从 AVFrameQueue 中连续取出音频帧播放
 */
class AudioOutput {
public:
    AudioOutput(AVSync* avsync, const AudioParams& audio_params,
        AVFrameQueue* frame_queue, AVRational time_base);
    ~AudioOutput();

    int Init();        // 初始化 SDL 和滤镜图
    int DeInit();      // 反初始化（关闭 SDL）

    void Pause();      // 暂停播放
    void Resume();     // 恢复播放
    bool isPaused();   // 是否处于暂停

    void SetSpeed(float s);     // 设置倍速
    float GetSpeed() const { return speed_; } // 获取当前倍速

public:
    AVFrameQueue* frame_queue_ = nullptr; // 音频帧队列（由外部提供）

    AudioParams src_tgt_; // 解码后源音频参数
    AudioParams dst_tgt_; // SDL 输出音频格式参数

    SwrContext* swr_ctx_ = nullptr; // 重采样上下文（如果 format/sample_rate 不一致才会使用）

    uint8_t* audio_buf1_ = nullptr; // 转换后的 PCM 缓冲区
    uint32_t audio_buf1_size = 0;
    uint8_t* audio_buf_ = nullptr;  // 当前正在播放的缓冲区
    uint32_t audio_buf_size = 0;
    uint32_t audio_buf_index = 0;   // 当前播放位置在 audio_buf_ 中的偏移

    AVRational time_base_; // 解码器音频时间基
    AVSync* avsync_ = nullptr; // 音视频同步对象
    double pts = 0;            // 当前播放时刻（秒）

    bool paused_ = false;      // 是否暂停

    float speed_ = 1.0f;       // 当前倍速
    int original_freq_ = 0;    // SDL 输出采样率

    // FFmpeg 滤镜图相关
    AVFilterGraph* filter_graph_ = nullptr;
    AVFilterContext* abuffer_ctx_ = nullptr;
    AVFilterContext* atempo_ctx_ = nullptr;
    AVFilterContext* abuffersink_ctx_ = nullptr;
};

#endif // AUDIOOUTPUT_H
