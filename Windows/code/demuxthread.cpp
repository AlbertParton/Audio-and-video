#include "demuxthread.h"
#include "maincontroller.h"
#include <cstdio>

extern "C" {
#include <libavutil/error.h>
}

/*
 * 备用空构造函数：
 * 用于先创建对象，再通过其它方式设置队列和 controller
 */
DemuxThread::DemuxThread() {};

/*
 * 常用构造函数
 * 外部传入音频队列 / 视频队列 / 控制器
 */
DemuxThread::DemuxThread(AVPacketQueue* audio_queue,
    AVPacketQueue* video_queue,
    MainController* controller)
{
    audio_queue_ = audio_queue;
    video_queue_ = video_queue;
    controller_ = controller;
};

/*
 * 析构函数
 * 停止线程并释放输入文件
 */
DemuxThread::~DemuxThread()
{
    Stop();

    // 释放输入媒体格式上下文
    if (ifmt_ctx_) {
        avformat_close_input(&ifmt_ctx_);
        ifmt_ctx_ = nullptr;
    }
};

/*
 * Init —— 打开媒体文件，查找音/视频流
 */
int DemuxThread::Init(const char* url)
{
    // 1. 参数检查：确保url不为空
    if (!url) {
        printf("%s(%d) url is null\n", __FUNCTION__, __LINE__);

        return -1;
    }

    // 2. 分配AVFormatContext
    ifmt_ctx_ = avformat_alloc_context();
    if (!ifmt_ctx_) {
        printf("%s(%d) avformat_alloc_context failed\n", __FUNCTION__, __LINE__);

        return -1;
    }

    // 3. 打开输入媒体文件
    int ret = avformat_open_input(&ifmt_ctx_, url, NULL, NULL);
    if (ret < 0) {
        // 将FFmpeg错误码转换为可读字符串
        av_strerror(ret, err2str_, sizeof(err2str_));
        printf("%s(%d) avformat_open_input failed:%d, %s\n",
            __FUNCTION__, __LINE__, ret, err2str_);

        return -1;
    }

    // 4. 查找流信息（读取包以确定流的编码参数）
    ret = avformat_find_stream_info(ifmt_ctx_, NULL);
    if (ret < 0) {
        av_strerror(ret, err2str_, sizeof(err2str_));
        printf("%s(%d) avformat_find_stream_info failed:%d, %s\n",
            __FUNCTION__, __LINE__, ret, err2str_);

        return -1;
    }

    // 5. 自动选择最佳音频/视频流
    // av_find_best_stream参数说明：
    //   ifmt_ctx_ - 媒体上下文
    //   AVMEDIA_TYPE_AUDIO/VIDEO - 媒体类型
    //   -1 - 自动选择流索引
    //   -1 - 没有相关流时返回-1
    //   NULL - 不使用解码器筛选
    //   0 - 标志位
    audio_stream_ = av_find_best_stream(ifmt_ctx_, AVMEDIA_TYPE_AUDIO, -1, -1, NULL, 0);
    video_stream_ = av_find_best_stream(ifmt_ctx_, AVMEDIA_TYPE_VIDEO, -1, -1, NULL, 0);

    // 检查是否找到音视频流
    if (audio_stream_ < 0 || video_stream_ < 0) {
        printf("no audio or no video stream found\n");

        return -1;
    }

    return 0;  // 初始化成功
};

/*
 * Start —— 启动后台线程
 */
int DemuxThread::Start()
{
    // 重置终止标志为false（确保线程可以运行）
    abort_.store(false);

    // 创建线程，将Run()方法作为线程函数
    // &DemuxThread::Run - 成员函数指针
    // this - 当前对象指针（作为隐含参数传递给成员函数）
    thread_ = std::thread(&DemuxThread::Run, this);

    return 0;  // 启动成功
};

/*
 * Stop —— 停止后台线程
 */
int DemuxThread::Stop()
{
    // 设置终止标志为true，通知线程退出
    abort_.store(true);

    // 如果线程是可连接的（即正在运行），等待其结束
    if (thread_.joinable()) {
        thread_.join();  // 阻塞直到线程结束
    }

    return 0;  // 停止成功
};

/* =================== Getter 接口 =================== */

AVFormatContext* DemuxThread::IfmtCtx()
{
    return ifmt_ctx_;
};

int DemuxThread::VideoStreamIndex()
{
    return video_stream_;
};

int DemuxThread::AudioStreamIndex()
{
    return audio_stream_;
};

AVCodecParameters* DemuxThread::AudioCodecParameters()
{
    if (audio_stream_ != -1 && ifmt_ctx_) {
        return ifmt_ctx_->streams[audio_stream_]->codecpar;
    }

    return nullptr;
};

AVCodecParameters* DemuxThread::VideoCodecParameters()
{
    if (video_stream_ != -1 && ifmt_ctx_) {
        return ifmt_ctx_->streams[video_stream_]->codecpar;
    }

    return nullptr;
};

AVRational DemuxThread::AudioStreamTimebase()
{
    if (audio_stream_ != -1 && ifmt_ctx_) {
        return ifmt_ctx_->streams[audio_stream_]->time_base;
    }

    return { 1, 1 };
};

AVRational DemuxThread::VideoStreamTimebase()
{
    if (video_stream_ != -1 && ifmt_ctx_) {
        return ifmt_ctx_->streams[video_stream_]->time_base;
    }

    return { 1, 1 };
};

/*
 * Run —— demux 主循环
 *
 * 逻辑：
 *   1. 等待暂停解除（controller 控制）
 *   2. 检查队列容量，避免堆积过多（>100）
 *   3. 调用 av_read_frame 读取 AVPacket
 *   4. 根据流索引分发到 audio/video 队列
 */
void DemuxThread::Run()
{
    AVPacket packet;  // 本地AVPacket变量（在栈上分配）
    int ret = 0;      // 返回值变量

    // 主循环：持续运行直到终止标志被设置
    while (!abort_.load()) {

        // ====== 暂停处理 ======
        // 检查播放器是否暂停，如果暂停则等待
        if (controller_ && controller_->isPaused()) {
            controller_->WaitIfPaused();  // 阻塞直到恢复播放
            continue;  // 继续下一次循环检查
        }

        // ====== 线程安全的队列指针获取 ======
        // 获取当前队列指针的本地副本，避免外部修改影响当前循环
        AVPacketQueue* local_aq = nullptr;
        AVPacketQueue* local_vq = nullptr;
        {
            // 加锁保护队列指针访问
            std::lock_guard<std::mutex> lk(queue_mtx_);
            local_aq = audio_queue_;
            local_vq = video_queue_;
        }

        // 检查队列指针是否有效
        if (!local_aq || !local_vq) {
            // 队列指针无效，短暂休眠后重试
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
            continue;
        }

        // ====== 流量控制：避免队列积压太大 ======
        // 如果队列中已有大量未处理的数据包，等待消费者处理
        if (local_aq->Size() > 100 || local_vq->Size() > 100) {
            // 队列较满，短暂休眠避免内存过度占用
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            continue;
        }

        // ====== 读取AVPacket ======
        // av_read_frame从媒体文件中读取下一个数据包
        // 返回0表示成功，<0表示错误或文件结束
        ret = av_read_frame(ifmt_ctx_, &packet);
        if (ret < 0) {
            // 读取失败：可能是文件结束（AVERROR_EOF）或其他错误
            char ebuf[128];
            av_strerror(ret, ebuf, sizeof(ebuf));
            printf("%s(%d) av_read_frame failed:%d, %s\n",
                __FUNCTION__, __LINE__, ret, ebuf);
            break;  // 退出主循环
        }

        // ====== 分发数据包到相应队列 ======
        // 根据数据包所属的流索引分发到对应的队列
        if (packet.stream_index == audio_stream_) {
            // 音频数据包：推入音频队列
            local_aq->Push(&packet);
        }
        else if (packet.stream_index == video_stream_) {
            // 视频数据包：推入视频队列
            local_vq->Push(&packet);
        }
        else {
            // 其他类型的数据包（如字幕）：直接释放
            av_packet_unref(&packet);
        }
    }
};
