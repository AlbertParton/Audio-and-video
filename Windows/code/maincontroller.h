#ifndef MAINCONTROLLER_H
#define MAINCONTROLLER_H

#include "demuxthread.h"
#include "decodethread.h"
#include "audiooutput.h"
#include "videooutput.h"
#include "avsync.h"
#include <thread>
#include <mutex>
#include <condition_variable>

/*
 * MainController
 *
 * 播放器核心控制类，负责整个播放器流程调度，包括：
 *   1. 初始化解复用 / 解码 / 音视频输出模块
 *   2. 控制播放 / 暂停 / 停止
 *   3. 控制倍速播放
 *   4. 控制线程生命周期
 *
 * 注意：
 *   - MainLoop() 会进入视频渲染阻塞循环（SDL 窗口）
 */
class MainController
{
public:
    // ================ 构造函数与析构函数 ================
    MainController(const char* url);  // 构造函数，传入视频文件路径
    ~MainController();                 // 析构函数，确保资源释放

    // ================ 播放器对外控制接口 ================
    // 被 main.cpp 调用，响应用户的键盘输入

    /**
     * @brief 开始播放（初始化 + 启动所有线程）
     * 功能：首次启动播放器，创建后台线程执行初始化工作
     */
    void start();

    /**
     * @brief 停止播放并释放所有资源
     * 功能：停止所有线程，清理资源，回到初始状态
     */
    void stop();

    /**
     * @brief 主渲染循环（阻塞至窗口关闭）
     * 功能：进入视频渲染主循环，由 VideoOutput 模块驱动
     */
    void MainLoop();

    /**
     * @brief 暂停播放
     * 功能：暂停所有处理线程，暂停视频和音频渲染
     */
    void pause();

    /**
     * @brief 恢复播放
     * 功能：恢复所有被暂停的线程，继续播放
     */
    void resume();

    // ================ 状态查询与设置接口 ================

    /**
     * @brief 获取当前播放速度
     * @return float 当前倍速值（1.0表示正常速度）
     */
    float getSpeed() const { return speed_; }

    /**
     * @brief 设置播放倍速
     * @param s 新的倍速值（0.5-1.0）
     * 功能：修改音频播放速度，视频通过时钟同步跟随
     */
    void setSpeed(float s);

    /**
     * @brief 检查是否处于暂停状态
     * @return bool 暂停状态（true=暂停，false=播放中）
     */
    bool isPaused() const;

    /**
     * @brief 检查播放器是否已启动
     * @return bool 启动状态（true=已启动，false=未启动）
     */
    bool isStarted() const { return started; }

    /**
     * @brief 如果暂停，则阻塞等待 resume()
     * 功能：被解复用线程和解码线程调用，实现暂停等待机制
     */
    void WaitIfPaused();

private:
    // ================ 内部辅助方法 ================
    // 仅在内部使用，封装了复杂的初始化逻辑

    /**
     * @brief 初始化所有模块
     * @return int 成功返回0，失败返回负值
     * 功能：创建并初始化所有播放组件
     */
    int InitAll();

    /**
     * @brief 启动所有处理线程
     * @return int 成功返回0，失败返回负值
     * 功能：启动解复用线程、音频解码线程、视频解码线程
     */
    int StartAllThreads();

    /**
     * @brief 停止所有线程并清理资源
     * 功能：按正确顺序停止所有组件，释放所有资源
     */
    void StopAndClean();

private:
    // ================ 成员变量 ================

    // 基本数据
    const char* m_url;                // 视频文件路径指针

    // ================ 数据队列 ================

    AVPacketQueue* audio_packet_queue; // 音频数据包队列
    AVPacketQueue* video_packet_queue; // 视频数据包队列
    AVFrameQueue* audio_frame_queue;   // 音频帧队列
    AVFrameQueue* video_frame_queue;   // 视频帧队列

    // ================ 同步与时钟 ================
    AVSync avsync;                    // 音视频同步时钟，以音频时钟为主时钟

    // ================ 播放线程模块 ================

    DemuxThread* demux_thread = nullptr;          // 解复用线程对象
    DecodeThread* audio_decode_thread = nullptr;  // 音频解码线程对象
    DecodeThread* video_decode_thread = nullptr;  // 视频解码线程对象

    // ================ 输出模块 ================
    AudioOutput* audio_output = nullptr;          // 音频输出模块（SDL音频）
    VideoOutput* video_output = nullptr;          // 视频输出模块（SDL窗口）

    // ================ 播放状态控制 ================
    bool started = false;    // 播放器启动标志（true=已启动，false=未启动）
    bool paused = false;     // 暂停状态标志（true=已暂停，false=播放中）

    std::thread play_thread; // 后台播放线程

    // ================ 暂停控制机制 ================
    // 使用条件变量实现暂停/恢复功能

    std::mutex pause_mtx;                   // 保护暂停状态的互斥锁
    std::condition_variable pause_cv;       // 暂停等待的条件变量

    // ================ 播放速度控制 ================
    float speed_ = 1.0f;                    // 当前播放速度（1.0=正常速度）
};

#endif // MAINCONTROLLER_H
