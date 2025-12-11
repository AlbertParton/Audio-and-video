#ifndef RENDER_H
#define RENDER_H

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#include <libavutil/avutil.h>
#include <libavutil/imgutils.h>
}

#include <thread>
#include <atomic>
#include <android/native_window.h>
#include "queue.h"
#include "fqueue.h"

class Render {
public:
    Render();

    ~Render();

    bool init(int src_width, int src_height, ANativeWindow *window);
    void setFrameQueue(FrameQueue *queue);
    void start();
    void stop();
    bool isFinished() const;

    void pause();
    void resume();

private:
    void renderThread();

    FrameQueue *frame_queue = nullptr;
    std::thread worker;
    std::atomic<bool> running;
    ANativeWindow *native_window = nullptr;
    SwsContext *sws_ctx = nullptr;
    AVFrame *rgb_frame = nullptr;
    uint8_t *rgb_buffer = nullptr;
    int width = 0;
    int height = 0;
    int dst_linesize[1];

    std::mutex pause_mutex;
    std::condition_variable pause_cond;
    bool paused;
};

#endif // RENDER_H
