#include "decodethread.h"
#include "maincontroller.h"

/**
 * @brief 构造函数
 */
DecodeThread::DecodeThread(AVPacketQueue* packet_queue,
    AVFrameQueue* frame_queue,
    MainController* controller)
    : packet_queue_(packet_queue),
    frame_queue_(frame_queue),
    controller_(controller)
{
    codec_ctx_ = nullptr;
};

/**
 * @brief 析构，停止线程并释放 codec_ctx_
 */
DecodeThread::~DecodeThread()
{
    Stop();

    if (codec_ctx_) {
        avcodec_free_context(&codec_ctx_);
        codec_ctx_ = nullptr;
    }
};

/**
 * @brief 初始化 FFmpeg 解码器
 */
int DecodeThread::Init(AVCodecParameters* par)
{
    // 1. 参数检查
    if (!par) {
        printf("DecodeThread::Init par is NULL\n");

        return -1;
    }

    // 2. 分配解码器上下文
    codec_ctx_ = avcodec_alloc_context3(NULL);

    // 3. 将流参数复制到解码器上下文
    int ret = avcodec_parameters_to_context(codec_ctx_, par);
    if (ret < 0) {
        // FFmpeg 错误码转换为可读字符串
        av_strerror(ret, err2str, sizeof(err2str));
        printf("avcodec_parameters_to_context failed, ret:%d, err:%s\n", ret, err2str);

        return -1;
    }

    // 4. 查找解码器（根据编解码ID）
    const AVCodec* codec = avcodec_find_decoder(codec_ctx_->codec_id);
    if (!codec) {
        printf("avcodec_find_decoder failed\n");

        return -1;
    }

    // 5. 打开解码器
    ret = avcodec_open2(codec_ctx_, codec, NULL);
    if (ret < 0) {
        av_strerror(ret, err2str, sizeof(err2str));
        printf("avcodec_open2 failed, ret:%d, err:%s\n", ret, err2str);

        return -1;
    }

    return 0;  // 初始化成功
};

/**
 * @brief 启动解码线程
 */
int DecodeThread::Start()
{
    // 创建新线程，执行 Run() 方法
    thread_ = new std::thread(&DecodeThread::Run, this);
    if (!thread_) {
        printf("new DecodeThread failed\n");
        return -1;
    }

    return 0;
};

/**
 * @brief 停止线程
 */
int DecodeThread::Stop()
{
    Thread::Stop();

    return 0;
};

/**
 * @brief 解码线程主循环
 */
void DecodeThread::Run()
{
    int ret = 0;
    // 预分配一个 AVFrame 用于接收解码结果
    AVFrame* frame = av_frame_alloc();

    // 主循环：持续解码直到终止
    while (1) {
        // 检查终止标志
        if (abort_ == 1)
            break;

        // ===== 暂停控制 =====
        // 如果播放器处于暂停状态，等待恢复
        if (controller_ && controller_->isPaused()) {
            controller_->WaitIfPaused();
        }

        // ===== 背压控制 =====
        // 输出队列过多时，等待消费者处理，避免内存占用过高
        if (frame_queue_->Size() > 10) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            continue;
        }

        // ===== 获取输入数据包 =====
        // 从队列中取出数据包，10ms超时避免忙等待
        AVPacket* packet = packet_queue_->Pop(10);
        if (packet) {
            // 有数据包，送入解码器
            ret = avcodec_send_packet(codec_ctx_, packet);
            // 立即释放数据包，解码器内部会复制数据
            av_packet_free(&packet);

            if (ret < 0) {
                // 解码错误处理
                av_strerror(ret, err2str, sizeof(err2str));
                printf("avcodec_send_packet failed, ret:%d, err:%s\n", ret, err2str);
                break;  // 严重错误，退出解码循环
            }

            // ===== 接收解码帧 =====
            // 一个 packet 可能产生多个 frame（如B帧场景）
            while (true) {
                ret = avcodec_receive_frame(codec_ctx_, frame);
                if (ret == 0) {
                    // 成功解码一帧，推入输出队列
                    frame_queue_->Push(frame);
                    // 注意：frame_queue_->Push() 会移动 frame 的引用
                    // 所以 frame 变为空，需要重新分配
                    frame = av_frame_alloc();  // 为下一次接收分配新frame
                    continue;
                }
                else if (ret == AVERROR(EAGAIN)) {
                    // 解码器需要更多输入，跳出接收循环
                    break;
                }
                else {
                    // 其他错误（如解码器内部错误、流结束等）
                    abort_ = 1;  // 设置终止标志
                    av_strerror(ret, err2str, sizeof(err2str));
                    printf("avcodec_receive_frame failed, ret:%d, err:%s\n", ret, err2str);
                    break;
                }
            }
        }
        else {
            // 队列为空，短暂休眠避免CPU空转
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
    }

    // ===== 清理资源 =====
    // 释放分配的 AVFrame
    if (frame) {
        av_frame_free(&frame);
    }
};

/**
 * @brief 清空解码器内部缓存（例如暂停过长后）
 */
void DecodeThread::Flush()
{
    if (codec_ctx_) {
        avcodec_flush_buffers(codec_ctx_);
    }
}

/**
 * @brief 获取解码器上下文
 */
AVCodecContext* DecodeThread::GetAVCodecContext()
{
    return codec_ctx_;
};
