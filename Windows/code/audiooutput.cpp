#include "audiooutput.h"
#include <cstring>
#include <cstdio>

/**
 * @brief SDL 音频回调函数
 *
 * SDL 每次需要音频数据时，都会调用此函数。
 * 回调内必须尽快返回，否则会导致音频卡顿。
 */
static void sdl_audio_callback(void* userdata, Uint8* stream, int len)
{
    AudioOutput* audio_output = (AudioOutput*)userdata;

    // 循环填充，直到满足 SDL 要求的长度
    while (len > 0) {
        // ---- 暂停时输出静音 ----
        if (audio_output->isPaused()) {
            memset(stream, 0, len);  // 填充静音（全0）
            return;  // 暂停时直接返回，不更新时钟
        }

        // ---- 若当前缓冲区播放完毕，则取下一帧 ----
        if (audio_output->audio_buf_index == audio_output->audio_buf_size) {
            // 重置播放位置
            audio_output->audio_buf_index = 0;

            // 从队列获取音频帧（2ms超时，避免阻塞）
            AVFrame* frame = audio_output->frame_queue_->Pop(2);
            AVFrame* filt_frame = nullptr;

            if (frame) {
                // 送入输入滤镜（原始音频帧）
                if (av_buffersrc_add_frame(audio_output->abuffer_ctx_, frame) < 0) {
                    // 添加失败，释放帧并继续
                    av_frame_free(&frame);
                    continue;
                }
                // 立即释放原始帧，滤镜内部已复制数据
                av_frame_free(&frame);

                // 从滤镜输出端取出处理后的帧（倍速处理）
                filt_frame = av_frame_alloc();
                if (av_buffersink_get_frame(audio_output->abuffersink_ctx_, filt_frame) < 0) {
                    // 获取失败，可能是滤镜内部缓冲不足
                    av_frame_free(&filt_frame);
                    audio_output->audio_buf_ = nullptr;
                    audio_output->audio_buf_size = 512;  // 设置默认静音长度
                    continue;
                }

                // 更新播放时间戳（PTS -> 秒）
                audio_output->pts = filt_frame->pts * av_q2d(audio_output->time_base_);

                // --------- 若滤镜输出参数不匹配 SDL，要进行重采样 ---------
                // 检查三个参数：采样格式、采样率、声道布局
                if (((filt_frame->format != audio_output->dst_tgt_.fmt)
                    || (filt_frame->sample_rate != audio_output->dst_tgt_.freq)
                    || av_channel_layout_compare(&filt_frame->ch_layout,
                        &audio_output->dst_tgt_.ch_layout) != 0)
                    && (!audio_output->swr_ctx_)) {  // 且重采样器未初始化

                    // 初始化 SwrContext（重采样器）
                    swr_alloc_set_opts2(
                        &audio_output->swr_ctx_,                    // 输出：重采样上下文
                        &audio_output->dst_tgt_.ch_layout,          // 目标声道布局
                        audio_output->dst_tgt_.fmt,                 // 目标采样格式
                        audio_output->dst_tgt_.freq,                // 目标采样率
                        &filt_frame->ch_layout,                     // 源声道布局
                        (enum AVSampleFormat)filt_frame->format,    // 源采样格式
                        filt_frame->sample_rate,                    // 源采样率
                        0,                                          // 日志偏移
                        nullptr                                     // 日志上下文
                    );

                    if (!audio_output->swr_ctx_ || swr_init(audio_output->swr_ctx_) < 0) {
                        printf("swr_init failed\n");
                        if (audio_output->swr_ctx_)
                            swr_free(&audio_output->swr_ctx_);
                        av_frame_free(&filt_frame);
                        return;  // 重采样初始化失败，直接返回
                    }
                }

                // --------- 重采样或直接复制 PCM 数据 ---------
                if (audio_output->swr_ctx_) {
                    // 需要重采样的情况（格式不匹配）
                    const uint8_t** in = (const uint8_t**)filt_frame->extended_data;
                    uint8_t** out = &audio_output->audio_buf1_;

                    // 计算输出样本数（考虑重采样率，+256 作为安全边界）
                    int out_samples =
                        filt_frame->nb_samples * audio_output->dst_tgt_.freq /
                        filt_frame->sample_rate + 256;

                    // 计算输出缓冲区大小（字节）
                    int out_bytes = av_samples_get_buffer_size(
                        nullptr,                                    // 行大小（不需要）
                        audio_output->dst_tgt_.ch_layout.nb_channels, // 声道数
                        out_samples,                                // 样本数
                        audio_output->dst_tgt_.fmt,                 // 采样格式
                        0                                           // 对齐方式
                    );

                    if (out_bytes < 0) {
                        av_frame_free(&filt_frame);
                        return;
                    }

                    // 动态分配/调整音频缓冲区大小
                    av_fast_malloc(&audio_output->audio_buf1_,
                        &audio_output->audio_buf1_size,
                        out_bytes);

                    // 执行重采样
                    int len2 = swr_convert(audio_output->swr_ctx_,
                        out, out_samples,      // 输出缓冲区
                        in, filt_frame->nb_samples); // 输入样本数

                    if (len2 < 0) {
                        av_frame_free(&filt_frame);
                        return;
                    }

                    // 获取实际转换后的数据大小
                    audio_output->audio_buf_size = av_samples_get_buffer_size(
                        nullptr,
                        audio_output->dst_tgt_.ch_layout.nb_channels,
                        len2,
                        audio_output->dst_tgt_.fmt, 0);

                    audio_output->audio_buf_ = audio_output->audio_buf1_;
                }
                else {
                    // 无需重采样，直接复制 PCM 数据
                    int out_bytes = av_samples_get_buffer_size(
                        nullptr,
                        filt_frame->ch_layout.nb_channels,
                        filt_frame->nb_samples,
                        (enum AVSampleFormat)filt_frame->format, 0);

                    av_fast_malloc(&audio_output->audio_buf1_,
                        &audio_output->audio_buf1_size,
                        out_bytes);

                    audio_output->audio_buf_ = audio_output->audio_buf1_;
                    audio_output->audio_buf_size = out_bytes;

                    // 复制数据（假设平面格式在 extended_data[0] 中）
                    memcpy(audio_output->audio_buf_,
                        filt_frame->extended_data[0],
                        out_bytes);
                }

                av_frame_free(&filt_frame);
            }
            else {
                // ---- 无帧时输出静音 ----
                // 队列为空，可能是解码较慢或文件结束
                audio_output->audio_buf_ = nullptr;
                audio_output->audio_buf_size = 512;  // 静音数据长度
            }
        }

        // ---- 计算可拷贝的长度 ----
        int len3 = audio_output->audio_buf_size - audio_output->audio_buf_index;
        if (len3 > len) len3 = len;  // 不能超过 SDL 请求的长度

        if (!audio_output->audio_buf_)
            memset(stream, 0, len3);  // 无数据，输出静音
        else
            memcpy(stream, audio_output->audio_buf_ + audio_output->audio_buf_index, len3);

        // 更新指针和剩余长度
        len -= len3;
        stream += len3;
        audio_output->audio_buf_index += len3;
    }

    // ---- 更新音频时钟用于同步 ----
    // 每次回调都更新时钟，确保视频同步准确
    audio_output->avsync_->SetClock(audio_output->pts);
};

// ============================================================================
//                                构造 / 析构
// ============================================================================

AudioOutput::AudioOutput(AVSync* avsync, const AudioParams& audio_params,
    AVFrameQueue* frame_queue, AVRational time_base)
    : avsync_(avsync),
    src_tgt_(audio_params),
    frame_queue_(frame_queue),
    time_base_(time_base)
{
    swr_ctx_ = nullptr;

    audio_buf1_ = nullptr;
    audio_buf1_size = 0;

    audio_buf_ = nullptr;
    audio_buf_size = 0;
    audio_buf_index = 0;

    paused_ = false;
    speed_ = 1.0f;
    original_freq_ = 0;

    filter_graph_ = nullptr;
    abuffer_ctx_ = nullptr;
    atempo_ctx_ = nullptr;
    abuffersink_ctx_ = nullptr;
};

AudioOutput::~AudioOutput()
{
    if (swr_ctx_) {
        swr_free(&swr_ctx_);
        swr_ctx_ = nullptr;
    }

    if (audio_buf1_) {
        av_free(audio_buf1_);
        audio_buf1_ = nullptr;
        audio_buf1_size = 0;
    }

    if (filter_graph_) {
        avfilter_graph_free(&filter_graph_);
        filter_graph_ = nullptr;
    }

    DeInit();
};

// ============================================================================
//                              Init / DeInit
// ============================================================================

int AudioOutput::Init()
{
    // 1. 初始化 SDL 音频子系统
    if (SDL_Init(SDL_INIT_AUDIO) != 0) {
        printf("SDL_Init failed\n");
        return -1;
    }

    // 2. 配置 SDL 音频参数
    SDL_AudioSpec wanted_spec;
    wanted_spec.channels = 2;                    // 固定为立体声
    wanted_spec.freq = src_tgt_.freq;            // 采样率（与源相同）
    wanted_spec.format = AUDIO_S16SYS;           // 有符号16位，系统字节序
    wanted_spec.silence = 0;                     // 静音值
    wanted_spec.callback = sdl_audio_callback;   // 回调函数
    wanted_spec.userdata = this;                 // 用户数据（this指针）
    wanted_spec.samples = 512;                   // 缓冲区样本数（越大延迟越大）

    // 打开音频设备
    if (SDL_OpenAudio(&wanted_spec, nullptr) != 0) {
        printf("SDL_OpenAudio failed\n");
        return -1;
    }

    // 3. 设置目标音频参数（SDL 实际输出格式）
    av_channel_layout_default(&dst_tgt_.ch_layout, wanted_spec.channels);
    dst_tgt_.fmt = AV_SAMPLE_FMT_S16;           // SDL 使用 S16 格式
    dst_tgt_.freq = wanted_spec.freq;           // SDL 采样率
    original_freq_ = wanted_spec.freq;          // 保存原始采样率（倍速时不变）

    // 4. 构建滤镜图（abuffer -> atempo -> abuffersink）
    filter_graph_ = avfilter_graph_alloc();

    // 构建 abuffer 滤镜参数
    char args[512];
    snprintf(args, sizeof(args),
        "sample_rate=%d:sample_fmt=%s:channel_layout=%" PRId64 ":time_base=1/%d",
        src_tgt_.freq,
        av_get_sample_fmt_name(src_tgt_.fmt),
        src_tgt_.ch_layout.u.mask,
        src_tgt_.freq);

    // 创建 abuffer 滤镜（输入源）
    const AVFilter* abuffer = avfilter_get_by_name("abuffer");
    avfilter_graph_create_filter(&abuffer_ctx_, abuffer, "src", args, nullptr, filter_graph_);

    // 创建 atempo 滤镜（倍速处理）
    char atempo_args[32];
    snprintf(atempo_args, sizeof(atempo_args), "tempo=%f", speed_);
    const AVFilter* atempo = avfilter_get_by_name("atempo");
    avfilter_graph_create_filter(&atempo_ctx_, atempo, "atempo", atempo_args, nullptr, filter_graph_);

    // 创建 abuffersink 滤镜（输出）
    const AVFilter* abuffersink = avfilter_get_by_name("abuffersink");
    avfilter_graph_create_filter(&abuffersink_ctx_, abuffersink, "sink", nullptr, nullptr, filter_graph_);

    // 连接滤镜：abuffer -> atempo -> abuffersink
    avfilter_link(abuffer_ctx_, 0, atempo_ctx_, 0);
    avfilter_link(atempo_ctx_, 0, abuffersink_ctx_, 0);

    // 配置滤镜图
    avfilter_graph_config(filter_graph_, nullptr);

    // 5. 启动音频播放（SDL_PauseAudio(0) 开始播放）
    SDL_PauseAudio(0);

    return 0;
};

int AudioOutput::DeInit()
{
    SDL_PauseAudio(1);
    SDL_CloseAudio();
    return 0;
};

// ============================================================================
//                                  Pause / Resume
// ============================================================================

void AudioOutput::Pause() { paused_ = true; };
void AudioOutput::Resume() { paused_ = false; };
bool AudioOutput::isPaused() { return paused_; };

// ============================================================================
//                                  SetSpeed
// ============================================================================

/**
 * @brief 设置音频倍速并重建滤镜图
 *
 * 限制：0.5x ~ 1.0x
 */
void AudioOutput::SetSpeed(float s)
{
    // 1. 参数范围限制
    if (s < 0.5f) s = 0.5f;

    // 相同倍速无需重建
    if (s == speed_)
        return;

    speed_ = s;

    // 2. 暂停音频播放（安全操作）
    SDL_PauseAudio(1);

    // 3. 销毁旧滤镜图
    if (filter_graph_) {
        avfilter_graph_free(&filter_graph_);
        filter_graph_ = nullptr;
        abuffer_ctx_ = nullptr;
        atempo_ctx_ = nullptr;
        abuffersink_ctx_ = nullptr;
    }

    // 4. 创建新的滤镜图
    filter_graph_ = avfilter_graph_alloc();
    if (!filter_graph_) {
        printf("avfilter_graph_alloc failed\n");
        SDL_PauseAudio(0);  // 恢复播放（尽管失败了）
        return;
    }

    // 重建 abuffer 滤镜（参数不变）
    char args[512];
    snprintf(args, sizeof(args),
        "sample_rate=%d:sample_fmt=%s:channel_layout=%" PRId64 ":time_base=1/%d",
        src_tgt_.freq,
        av_get_sample_fmt_name(src_tgt_.fmt),
        src_tgt_.ch_layout.u.mask,
        src_tgt_.freq);

    const AVFilter* abuffer = avfilter_get_by_name("abuffer");
    if (avfilter_graph_create_filter(&abuffer_ctx_, abuffer, "src", args, nullptr, filter_graph_) < 0) {
        printf("create abuffer filter failed\n");
        avfilter_graph_free(&filter_graph_);
        SDL_PauseAudio(0);
        return;
    }

    // 重建 atempo 滤镜（使用新的倍速参数）
    char atempo_args[32];
    snprintf(atempo_args, sizeof(atempo_args), "tempo=%f", speed_);
    const AVFilter* atempo = avfilter_get_by_name("atempo");
    if (avfilter_graph_create_filter(&atempo_ctx_, atempo, "atempo", atempo_args, nullptr, filter_graph_) < 0) {
        printf("create atempo filter failed\n");
        avfilter_graph_free(&filter_graph_);
        SDL_PauseAudio(0);
        return;
    }

    // 重建 abuffersink 滤镜
    const AVFilter* abuffersink = avfilter_get_by_name("abuffersink");
    if (avfilter_graph_create_filter(&abuffersink_ctx_, abuffersink, "sink", nullptr, nullptr, filter_graph_) < 0) {
        printf("create abuffersink filter failed\n");
        avfilter_graph_free(&filter_graph_);
        SDL_PauseAudio(0);
        return;
    }

    // 连接滤镜
    if (avfilter_link(abuffer_ctx_, 0, atempo_ctx_, 0) < 0 ||
        avfilter_link(atempo_ctx_, 0, abuffersink_ctx_, 0) < 0) {
        printf("link filters failed\n");
        avfilter_graph_free(&filter_graph_);
        SDL_PauseAudio(0);
        return;
    }

    // 配置滤镜图
    if (avfilter_graph_config(filter_graph_, nullptr) < 0) {
        printf("config filter graph failed\n");
        avfilter_graph_free(&filter_graph_);
        SDL_PauseAudio(0);
        return;
    }

    // 5. 重置音频缓冲区状态（避免使用旧数据）
    audio_buf_index = 0;
    audio_buf_size = 0;
    audio_buf_ = nullptr;

    printf("Filter graph rebuilt for speed: %fx\n", speed_);

    // 6. 恢复音频播放
    SDL_PauseAudio(0);
};
