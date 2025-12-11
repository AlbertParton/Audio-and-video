#include "demuxer.h"
#include <android/log.h>

#define LOG_TAG "Demuxer"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

Demuxer::Demuxer() {
    running = false;
}

Demuxer::~Demuxer() {
    stop();

    if (fmt_ctx) {
        avformat_close_input(&fmt_ctx);
    }
}

bool Demuxer::open(const char *filepath) {
    //打开输入文件
    avformat_open_input(&fmt_ctx, filepath, nullptr, nullptr);

    //查找流信息
    avformat_find_stream_info(fmt_ctx, nullptr);

    // 遍历找到视频流
    for (unsigned int i = 0; i < fmt_ctx->nb_streams; ++i) {
        if (fmt_ctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
            video_stream_index = i;

            break;
        }
    }

    if (video_stream_index == -1) {
        LOGE("No video stream found");

        return false;
    }

    LOGI("Video stream index: %d", video_stream_index);

    return true;
}

void Demuxer::start() {
    running = true;

    //在新的线程中解复用
    worker = std::thread(&Demuxer::demuxThread, this);
}

void Demuxer::stop() {
    running = false;

    //中止队列中的所有操作
    if (video_queue) {
        video_queue->abort();
    }

    if (worker.joinable()) {
        worker.join();
    }
}

//存放从视频流中解复用出来的 AVPacket 数据包
void Demuxer::setQueue(PacketQueue *queue) {
    video_queue = queue;
}

int Demuxer::getVideoStreamIndex() const {
    return video_stream_index;
}

AVCodecParameters *Demuxer::getVideoCodecParams() {
    //返回编解码参数
    if (video_stream_index < 0 || !fmt_ctx)
        return nullptr;

    return fmt_ctx->streams[video_stream_index]->codecpar;
}

bool Demuxer::isFinished() const {
    return !running;
}

void Demuxer::demuxThread() {
    AVPacket *packet = av_packet_alloc();

    while (running) {
        //读取 AVPacket 数据包
        if (av_read_frame(fmt_ctx, packet) < 0) {
            LOGI("End of stream");

            break;
        }

        //将 AVPacket 入队
        if (packet->stream_index == video_stream_index) {
            AVPacket *new_pkt = av_packet_alloc();
            av_packet_ref(new_pkt, packet);
            video_queue->push(new_pkt);
        }

        av_packet_unref(packet);
    }

    av_packet_free(&packet);
    running = false;
    LOGI("Demux thread finished");
}
