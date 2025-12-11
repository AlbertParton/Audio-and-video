#include "videooutput.h"
#include <thread>

#define REFRESH_RATE 0.01  // 刷新间隔（秒）

// 固定窗口大小
#define WINDOW_W 1280
#define WINDOW_H 720

// ---------------------------------------------------------
// 构造与析构
// ---------------------------------------------------------
VideoOutput::VideoOutput(AVSync* avsync, AVFrameQueue* frame_queue,
    int video_width, int video_height, AVRational time_base)
    : avsync_(avsync),
    frame_queue_(frame_queue),
    video_width_(video_width),
    video_height_(video_height),
    time_base_(time_base)
{};

VideoOutput::~VideoOutput()
{
    if (texture_) {
        SDL_DestroyTexture(texture_);
        texture_ = nullptr;
    }
    if (renderer_) {
        SDL_DestroyRenderer(renderer_);
        renderer_ = nullptr;
    }
    if (win_) {
        SDL_DestroyWindow(win_);
        win_ = nullptr;
    }
};

// ---------------------------------------------------------
// 初始化 / 释放
// ---------------------------------------------------------
int VideoOutput::Init()
{
    // 1. 初始化 SDL 视频子系统
    if (SDL_Init(SDL_INIT_VIDEO)) {
        printf("SDL_Init failed: %s\n", SDL_GetError());
        return -1;
    }

    // 2. 创建窗口
    // 参数说明：
    // - "player": 窗口标题
    // - SDL_WINDOWPOS_CENTERED: 窗口水平居中
    // - SDL_WINDOWPOS_CENTERED: 窗口垂直居中
    // - WINDOW_W, WINDOW_H: 窗口宽度和高度
    // - SDL_WINDOW_OPENGL: 使用 OpenGL 后端（无 SDL_WINDOW_RESIZABLE 标志）
    win_ = SDL_CreateWindow("player",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        WINDOW_W, WINDOW_H,
        SDL_WINDOW_OPENGL);

    if (!win_) {
        printf("SDL_CreateWindow failed: %s\n", SDL_GetError());
        return -1;
    }

    // 3. 创建渲染器
    // 参数说明：
    // - win_: 关联的窗口
    // - -1: 使用第一个支持的渲染驱动
    // - SDL_RENDERER_ACCELERATED: 使用硬件加速
    renderer_ = SDL_CreateRenderer(win_, -1, SDL_RENDERER_ACCELERATED);
    if (!renderer_) {
        printf("SDL_CreateRenderer failed: %s\n", SDL_GetError());
        return -1;
    }

    // 4. 创建 YUV 纹理
    // 参数说明：
    // - renderer_: 关联的渲染器
    // - SDL_PIXELFORMAT_IYUV: YUV420 格式（I420）
    // - SDL_TEXTUREACCESS_STREAMING: 流式纹理，需要频繁更新
    // - video_width_, video_height_: 纹理尺寸（视频原始尺寸）
    texture_ = SDL_CreateTexture(renderer_, SDL_PIXELFORMAT_IYUV,
        SDL_TEXTUREACCESS_STREAMING,
        video_width_, video_height_);

    if (!texture_) {
        printf("SDL_CreateTexture failed: %s\n", SDL_GetError());
        return -1;
    }

    return 0;  // 初始化成功
};

void VideoOutput::DeInit()
{
    if (texture_) { SDL_DestroyTexture(texture_); texture_ = nullptr; }
    if (renderer_) { SDL_DestroyRenderer(renderer_); renderer_ = nullptr; }
    if (win_) { SDL_DestroyWindow(win_); win_ = nullptr; }

    SDL_Quit();
};

// ---------------------------------------------------------
// 主事件循环
// ---------------------------------------------------------
int VideoOutput::MainLoop()
{
    SDL_Event event;  // SDL 事件对象

    while (true) {
        // 等待事件并刷新视频
        RefreshLoopWaitEvent(&event);

        // 处理事件
        switch (event.type) {
        case SDL_KEYDOWN:
            // ESC 键退出
            if (event.key.keysym.sym == SDLK_ESCAPE) {
                printf("ESC pressed, exit\n");
                return 0;  // 退出主循环
            }
            break;

        case SDL_QUIT:
            // 窗口关闭事件
            printf("SDL_QUIT received\n");
            return 0;  // 退出主循环

        default:
            // 其他事件（鼠标移动、窗口事件等）忽略
            break;
        }
    }
};

// ---------------------------------------------------------
// 刷新循环：等待 SDL 事件，同时按固定间隔刷新画面
// ---------------------------------------------------------
void VideoOutput::RefreshLoopWaitEvent(SDL_Event* event)
{
    double remain_time = 0.0;  // 需要休眠的时间（秒）

    // 从系统获取新事件到事件队列
    SDL_PumpEvents();

    // 循环直到收到事件
    while (!SDL_PeepEvents(event, 1, SDL_GETEVENT, SDL_FIRSTEVENT, SDL_LASTEVENT)) {
        // SDL_PeepEvents 参数说明：
        // - event: 存储事件的位置
        // - 1: 最多检查一个事件
        // - SDL_GETEVENT: 检查事件但不从队列移除
        // - SDL_FIRSTEVENT, SDL_LASTEVENT: 检查所有类型的事件

        // 如果有需要休眠的时间，则休眠
        if (remain_time > 0.0) {
            std::this_thread::sleep_for(
                std::chrono::milliseconds(int64_t(remain_time * 1000)));
        }

        // 刷新视频（可能更新 remain_time）
        videoRefresh(remain_time);

        // 再次获取新事件
        SDL_PumpEvents();
    }

    // 循环结束：收到事件，事件已存储在 event 参数中
};

// ---------------------------------------------------------
// 按比例缩放 + 居中绘制
// ---------------------------------------------------------
static SDL_Rect CalcLetterBoxRect(int video_w, int video_h)
{
    double window_w = WINDOW_W;
    double window_h = WINDOW_H;

    // 计算缩放比例（保持宽高比）
    double scale = std::min(window_w / video_w, window_h / video_h);

    int draw_w = video_w * scale;
    int draw_h = video_h * scale;

    // 居中
    int x = (window_w - draw_w) / 2;
    int y = (window_h - draw_h) / 2;

    SDL_Rect rect = { x, y, draw_w, draw_h };
    return rect;
};

// ---------------------------------------------------------
// 刷新视频帧：处理 A/V 同步并渲染 YUV 帧
// ---------------------------------------------------------
void VideoOutput::videoRefresh(double& remain_time)
{
    // 1. 暂停状态：保持刷新节奏但不渲染
    if (paused_) {
        remain_time = REFRESH_RATE;  // 10ms 后再次检查
        return;
    }

    // 2. 获取队列中的下一帧（不弹出）
    AVFrame* frame = frame_queue_->Front();
    if (!frame) {
        // 队列为空，等待下一轮刷新
        remain_time = REFRESH_RATE;
        return;
    }

    // 3. A/V 同步计算
    // 将帧的 PTS 转换为秒
    double pts = frame->pts * av_q2d(time_base_);

    // 计算帧显示时间与音频时钟的差值
    // diff > 0: 帧应该在未来显示（还没到时间）
    // diff <= 0: 帧应该现在或过去显示（可以/应该立即显示）
    double diff = pts - avsync_->GetClock();

    // 4. 如果帧还没到显示时间，计算需要等待的时间
    if (diff > 0) {
        // 限制最大等待时间为 REFRESH_RATE（10ms）
        // 避免长时间阻塞事件处理
        remain_time = (diff > REFRESH_RATE ? REFRESH_RATE : diff);
        return;  // 不渲染，等待下一轮
    }

    // 5. 计算渲染位置（Letterbox 缩放）
    SDL_Rect rect = CalcLetterBoxRect(video_width_, video_height_);

    // 6. 更新 YUV 纹理
    // 参数说明：
    // - texture_: 目标纹理
    // - NULL: 更新整个纹理（不是部分更新）
    // - frame->data[0]: Y 平面数据
    // - frame->linesize[0]: Y 平面行大小
    // - frame->data[1]: U 平面数据
    // - frame->linesize[1]: U 平面行大小
    // - frame->data[2]: V 平面数据
    // - frame->linesize[2]: V 平面行大小
    SDL_UpdateYUVTexture(texture_, NULL,
        frame->data[0], frame->linesize[0],
        frame->data[1], frame->linesize[1],
        frame->data[2], frame->linesize[2]);

    // 7. 清屏（填充黑色背景，形成 Letterbox 的黑边）
    SDL_SetRenderDrawColor(renderer_, 0, 0, 0, 255);  // 黑色，不透明
    SDL_RenderClear(renderer_);  // 清除渲染目标为当前绘制颜色

    // 8. 渲染缩放后的图像
    // 参数说明：
    // - renderer_: 渲染器
    // - texture_: 源纹理
    // - NULL: 使用整个纹理
    // - &rect: 目标矩形（Letterbox 位置和大小）
    SDL_RenderCopy(renderer_, texture_, NULL, &rect);

    // 9. 显示到屏幕（双缓冲交换）
    SDL_RenderPresent(renderer_);

    // 10. 从队列弹出并释放已渲染的帧
    // 注意：这里先弹出再释放，确保帧不再使用
    frame = frame_queue_->Pop(1);  // 1ms 超时
    if (frame) {
        av_frame_free(&frame);  // 释放帧资源
    }

    // 11. 设置下次刷新时间（立即刷新下一帧）
    remain_time = 0.0;
};

// ---------------------------------------------------------
// 暂停控制
// ---------------------------------------------------------
void VideoOutput::Pause() { paused_ = true; };
void VideoOutput::Resume() { paused_ = false; };
bool  VideoOutput::isPaused() { return paused_; };
