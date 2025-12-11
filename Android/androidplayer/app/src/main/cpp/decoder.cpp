#include "decoder.h"
#include <android/log.h>
#include <chrono>
#include <thread>

#define LOG_TAG "Decoder"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

Decoder::Decoder() {
    running = false;
}

Decoder::~Decoder() {
    stop();

    //释放解码器上下文
    if (codec_ctx) {
        avcodec_free_context(&codec_ctx);
    }

    if (yuv_out) {
        fclose(yuv_out);
    }
}

bool Decoder::init(AVCodecParameters *codecpar) {
    //查找对应的解码器
    const AVCodec *decoder = avcodec_find_decoder(codecpar->codec_id);

    //分配解码器上下文
    codec_ctx = avcodec_alloc_context3(decoder);

    //将视频流的参数复制到解码器上下文
    avcodec_parameters_to_context(codec_ctx, codecpar);

    //打开解码器
    avcodec_open2(codec_ctx, decoder, nullptr);

    width = codec_ctx->width;
    height = codec_ctx->height;
    LOGI("Decoder initialized: %dx%d", width, height);

    return true;
}

//设置解码器的输入队列
void Decoder::setQueue(PacketQueue *queue) {
    video_queue = queue;
}

//设置YUV 文件路径
void Decoder::setOutputFile(const char *path) {
    yuv_out = fopen(path, "wb");
}

// 设置帧输出队列
void Decoder::setFrameQueue(FrameQueue *queue) {
    frame_queue = queue;
}

void Decoder::start() {
    running = true;
    worker = std::thread(&Decoder::decodeThread, this);
}

void Decoder::stop() {
    running = false;

    if (worker.joinable()) {
        worker.join();
    }
}

void Decoder::decodeThread() {
    AVFrame *frame = av_frame_alloc();

    while (running) {
        //获取一个数据包
        AVPacket *pkt = video_queue->pop();
        if (!pkt) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));

            continue;
        }

        //将数据包发送给解码器
        if (avcodec_send_packet(codec_ctx, pkt) < 0) {
            av_packet_free(&pkt);

            continue;
        }

        //获取解码后的帧
        while (avcodec_receive_frame(codec_ctx, frame) == 0) {
            // 如果设置了输出文件，写入YUV数据
            if (yuv_out) {
                int y_size = width * height;
                int uv_size = y_size / 4;

                fwrite(frame->data[0], 1, y_size, yuv_out); // 亮度信息
                fwrite(frame->data[1], 1, uv_size, yuv_out); // 色度信息
                fwrite(frame->data[2], 1, uv_size, yuv_out); // 色度信息
            }

            if (frame_queue) {
                AVFrame *frame_copy = av_frame_clone(frame);
                if (frame_copy) {
                    frame_queue->push(frame_copy);
                } else {
                    LOGE("Failed to clone frame");
                }
            }
        }

        av_packet_free(&pkt);
    }

    av_frame_free(&frame);
    LOGI("Decoder thread finished");
}

bool Decoder::isFinished() const {
    return !running;
}
