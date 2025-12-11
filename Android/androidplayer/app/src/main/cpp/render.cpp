#include "render.h"
#include "libavutil/imgutils.h"
#include <android/log.h>
#include <chrono>
#include <thread>
#include <android/native_window.h>

#define LOG_TAG "Renderer"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

Render::Render() {
    running = false;
    paused = false; // 初始化暂停状态
}

Render::~Render() {
    stop();

    if (sws_ctx) {
        sws_freeContext(sws_ctx);
        sws_ctx = nullptr;
    }
    if (rgb_frame) {
        av_frame_free(&rgb_frame);
    }
    if (rgb_buffer) {
        av_free(rgb_buffer);
        rgb_buffer = nullptr;
    }
    if (native_window) {
        ANativeWindow_release(native_window);
        native_window = nullptr;
    }
}

bool Render::init(int src_width, int src_height, ANativeWindow *window) {
    native_window = window;
    width = src_width;
    height = src_height;

    // 设置窗口缓冲区尺寸和格式
    int result = ANativeWindow_setBuffersGeometry(native_window, width, height, WINDOW_FORMAT_RGBA_8888);

    // 分配RGB转换缓冲区
    int buffer_size = width * height * 4;  // RGBA 每个像素 4 字节
    rgb_buffer = (uint8_t*) av_malloc(buffer_size * sizeof(uint8_t));  // 分配内存

    // 初始化RGB帧
    rgb_frame = av_frame_alloc();

    av_image_fill_arrays(rgb_frame->data, rgb_frame->linesize, rgb_buffer,
                         AV_PIX_FMT_RGBA, width, height, 1);

    // 创建SWS转换上下文
    sws_ctx = sws_getContext(
            width, height, AV_PIX_FMT_YUV420P,
            width, height, AV_PIX_FMT_RGBA,
            SWS_BICUBIC, nullptr, nullptr, nullptr
    );

    LOGI("Renderer initialized: %dx%d", width, height);
    return true;
}

void Render::setFrameQueue(FrameQueue *queue) {
    frame_queue = queue;
}

void Render::start() {
    running = true;

    worker = std::thread(&Render::renderThread, this);
}

void Render::stop() {
    // 确保线程不会卡在暂停状态
    if (paused) {
        resume();
    }

    running = false;
    pause_cond.notify_all(); // 唤醒所有等待线程

    if (worker.joinable()) {
        worker.join();
    }
}

// 暂停函数
void Render::pause() {
    std::lock_guard<std::mutex> lock(pause_mutex);
    paused = true;
    LOGI("Renderer paused");
}

// 恢复函数
void Render::resume() {
    std::lock_guard<std::mutex> lock(pause_mutex);
    paused = false;
    pause_cond.notify_all(); // 唤醒所有等待线程
    LOGI("Renderer resumed");
}

void Render::renderThread() {
    LOGI("Renderer thread started");

    while (running) {
        // 检查暂停状态
        {
            std::unique_lock<std::mutex> lock(pause_mutex);
            pause_cond.wait(lock, [this] {
                return !paused || !running; // 条件：非暂停或停止运行
            });
            if (!running) break; // 检查是否要退出
        }

        AVFrame *frame = frame_queue->pop();
        if (!frame) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            continue;
        }

        // 转换YUV到RGB
        sws_scale(sws_ctx,
                  frame->data, frame->linesize,
                  0, height,
                  rgb_frame->data, rgb_frame->linesize);

        // 渲染到NativeWindow
        ANativeWindow_Buffer buffer;
        if (ANativeWindow_lock(native_window, &buffer, nullptr) < 0) {
            LOGE("ANativeWindow_lock failed");
            av_frame_free(&frame);
            continue;
        }

        // 将RGB数据复制到窗口缓冲区
        uint8_t *dst = static_cast<uint8_t*>(buffer.bits);
        int dst_stride = buffer.stride * 4;
        uint8_t *src = rgb_frame->data[0];
        int src_stride = rgb_frame->linesize[0];

        for (int i = 0; i < height; i++) {
            memcpy(dst + i * dst_stride, src + i * src_stride, src_stride);
        }

        ANativeWindow_unlockAndPost(native_window);
        av_frame_free(&frame);
    }

    LOGI("Renderer thread finished");
}

bool Render::isFinished() const {
    return !running;
}