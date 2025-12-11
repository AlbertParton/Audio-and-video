#ifndef VIDEOOUTPUT_H
#define VIDEOOUTPUT_H

#include "avframequeue.h"
#include "avsync.h"

#ifdef __cplusplus
extern "C" {
#include "SDL.h"
}
#endif

/**
 * @brief 视频输出类：负责创建窗口、渲染帧、处理暂停状态和视频刷新逻辑。
 */
class VideoOutput
{
public:
    /**
     * @brief 构造函数
     * @param avsync         音视频同步模块
     * @param frame_queue    视频帧队列
     * @param video_width    视频宽
     * @param video_height   视频高
     * @param time_base      视频流时间基
     */
    VideoOutput(AVSync* avsync, AVFrameQueue* frame_queue,
        int video_width, int video_height, AVRational time_base);

    ~VideoOutput();

    int Init();                        // 初始化 SDL、创建窗口/渲染器/纹理
    void DeInit();                     // 释放 SDL 资源

    int MainLoop();                    // 主事件循环（按 ESC / 关闭窗口退出）
    void RefreshLoopWaitEvent(SDL_Event* event);   // 刷新循环 + 等待事件

    void Pause();                      // 暂停播放
    void Resume();                     // 恢复播放
    bool isPaused();                   // 是否暂停

private:
    void videoRefresh(double& remain_time);  // 刷新一帧视频，执行同步与渲染逻辑

private:
    AVFrameQueue* frame_queue_ = nullptr;    // 视频帧队列
    SDL_Window* win_ = nullptr;              // 播放窗口
    SDL_Renderer* renderer_ = nullptr;       // SDL 渲染器
    SDL_Texture* texture_ = nullptr;         // SDL YUV 纹理

    int video_width_ = 0;                    // 视频宽度
    int video_height_ = 0;                   // 视频高度
    AVRational time_base_;                   // 时间基
    AVSync* avsync_ = nullptr;               // 音视频同步对象

    bool paused_ = false;                    // 是否暂停播放
};

#endif // VIDEOOUTPUT_H
