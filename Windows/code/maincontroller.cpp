#include "maincontroller.h"
#include <cstdio>
#include <cstring>

/*
 * 构造函数
 * 创建音视频的 Packet/Frame 队列
 */
MainController::MainController(const char* url)
{
    m_url = url;  // 保存视频文件路径（指针赋值）

    // 创建四个队列对象，用于线程间数据传递
    audio_packet_queue = new AVPacketQueue();  // 音频包队列
    video_packet_queue = new AVPacketQueue();  // 视频包队列
    audio_frame_queue = new AVFrameQueue();    // 音频帧队列
    video_frame_queue = new AVFrameQueue();    // 视频帧队列
};

/*
 * 析构：停止播放并清理所有资源
 */
MainController::~MainController()
{
    stop();
};

/*===================================================================
 *                        播放控制接口
 *===================================================================*/

 /*
  * start()
  * 创建后台线程，内部执行以下步骤：
  *   1. InitAll() 初始化所有模块
  *   2. StartAllThreads() 启动解复用+解码
  *   3. MainLoop() 进入视频渲染主循环
  */
void MainController::start()
{
    // 检查是否已启动，避免重复启动
    if (started) return;

    // 设置启动标志
    started = true;

    // 创建后台播放线程
    play_thread = std::thread([this]() {
        // 步骤1：初始化所有模块
        if (InitAll() < 0) {
            printf("InitAll failed\n");
            started = false;  // 初始化失败，重置标志
            return;
        }

        // 步骤2：启动所有处理线程（解复用+解码）
        if (StartAllThreads() < 0) {
            printf("StartAllThreads failed\n");
            started = false;
            return;
        }

        // 步骤3：进入主渲染循环（阻塞直到窗口关闭）
        MainLoop();
        });

    // 分离线程，使其独立运行
    play_thread.detach();
};

/*
 * 暂停播放
 */
void MainController::pause()
{
    if (!started)
        return;

    paused = true;
};

/*
 * 恢复播放
 */
void MainController::resume()
{
    if (!started)
        return;

    {
        // 加锁修改暂停标志
        std::lock_guard<std::mutex> lk(pause_mtx);
        paused = false;
    }

    // 通知所有等待的线程
    pause_cv.notify_all();
};

/*
 * 设置倍速（只影响音频，视频依赖时钟同步）
 */
void MainController::setSpeed(float s)
{
    speed_ = s;

    if (audio_output)
        audio_output->SetSpeed(s);
};

/*
 * 返回暂停状态
 */
bool MainController::isPaused() const
{
    return paused;
};

/*
 * 若已暂停，则阻塞等待恢复
 */
void MainController::WaitIfPaused()
{
    // 创建唯一锁
    std::unique_lock<std::mutex> lk(pause_mtx);

    // 等待条件：暂停状态解除或播放器停止
    pause_cv.wait(lk, [this]() {
        return !paused || !started;
        });
};

/*
 * 停止播放（对外接口）
 */
void MainController::stop()
{
    StopAndClean();
};

/*
 * 主渲染循环：由 VideoOutput 驱动（SDL 窗口）
 * 直到窗口关闭后退出，并执行清理
 */
void MainController::MainLoop()
{
    if (video_output)
        video_output->MainLoop(); // 内部阻塞直到退出

    stop();
};

/*
 * 按顺序创建和初始化所有播放模块
 * 初始化顺序：
 *   1. 解复用器 (DemuxThread)
 *   2. 音频解码器 (DecodeThread)
 *   3. 视频解码器 (DecodeThread)
 *   4. 同步时钟 (AVSync)
 *   5. 音频输出模块 (AudioOutput)
 *   6. 视频输出模块 (VideoOutput)
 */
int MainController::InitAll()
{
    int ret = 0;  // 返回值

    /*--------------------- 1. 解复用器初始化 ---------------------*/
    demux_thread = new DemuxThread(audio_packet_queue, video_packet_queue, this);
    ret = demux_thread->Init(m_url);  // 打开媒体文件，查找音视频流
    if (ret < 0) {
        printf("%s(%d) demux_thread Init failed\n", __FUNCTION__, __LINE__);

        return ret;  // 初始化失败，直接返回
    }

    /*--------------------- 2. 音频解码器初始化 ---------------------*/
    audio_decode_thread = new DecodeThread(audio_packet_queue, audio_frame_queue, this);
    // 获取音频流参数并初始化解码器
    ret = audio_decode_thread->Init(demux_thread->AudioCodecParameters());
    if (ret < 0) {
        printf("%s(%d) audio_decode_thread Init failed\n", __FUNCTION__, __LINE__);

        return ret;
    }

    /*--------------------- 3. 视频解码器初始化 ---------------------*/
    video_decode_thread = new DecodeThread(video_packet_queue, video_frame_queue, this);
    // 获取视频流参数并初始化解码器
    ret = video_decode_thread->Init(demux_thread->VideoCodecParameters());
    if (ret < 0) {
        printf("%s(%d) video_decode_thread Init failed\n", __FUNCTION__, __LINE__);

        return ret;
    }

    /*--------------------- 4. 同步时钟初始化 ---------------------*/
    avsync.InitClock();  // 初始化音频时钟为主时钟

    /*--------------------- 5. 音频输出模块初始化 ---------------------*/
    // 准备音频参数结构体
    AudioParams audio_params;
    memset(&audio_params, 0, sizeof(audio_params));

    // 从音频解码器获取音频参数
    audio_params.ch_layout = audio_decode_thread->GetAVCodecContext()->ch_layout;  // 声道布局
    audio_params.fmt = audio_decode_thread->GetAVCodecContext()->sample_fmt;        // 采样格式
    audio_params.freq = audio_decode_thread->GetAVCodecContext()->sample_rate;      // 采样率

    // 创建音频输出模块
    audio_output = new AudioOutput(
        &avsync,                          // 同步时钟
        audio_params,                     // 音频参数
        audio_frame_queue,                // 音频帧队列
        demux_thread->AudioStreamTimebase()  // 音频时间基
    );

    // 初始化音频输出（打开SDL音频设备）
    ret = audio_output->Init();
    if (ret < 0) {
        printf("%s(%d) audio_output Init failed\n", __FUNCTION__, __LINE__);

        return ret;
    }

    /*--------------------- 6. 视频输出模块初始化 ---------------------*/
    video_output = new VideoOutput(
        &avsync,                          // 同步时钟
        video_frame_queue,                // 视频帧队列
        video_decode_thread->GetAVCodecContext()->width,     // 视频宽度
        video_decode_thread->GetAVCodecContext()->height,    // 视频高度
        demux_thread->VideoStreamTimebase()  // 视频时间基
    );

    // 初始化视频输出（创建SDL窗口）
    ret = video_output->Init();
    if (ret < 0) {
        printf("%s(%d) video_output Init failed\n", __FUNCTION__, __LINE__);

        return ret;
    }

    return 0;  // 所有初始化成功
};

/*
 * 按顺序启动解复用线程和音视频解码线程
 */
int MainController::StartAllThreads()
{
    int ret = 0;

    // 启动解复用线程（数据源）
    if ((ret = demux_thread->Start()) < 0)
        return ret;

    // 启动音频解码线程（消费者1）
    if ((ret = audio_decode_thread->Start()) < 0)
        return ret;

    // 启动视频解码线程（消费者2）
    if ((ret = video_decode_thread->Start()) < 0)
        return ret;

    return 0;  // 所有线程启动成功
};

/*
 * 按正确的顺序停止所有组件，释放所有资源
 */
void MainController::StopAndClean()
{
    // 如果播放器未启动，直接返回
    if (!started)
        return;

    /*------------- 1. 解除暂停状态（避免阻塞） -------------*/
    {
        // 加锁修改暂停状态
        std::lock_guard<std::mutex> lk(pause_mtx);
        paused = false;  // 强制设置为非暂停状态
    }
    // 通知所有等待在条件变量上的线程
    pause_cv.notify_all();

    /*------------- 2. 停止处理线程（解码→解复用） -------------*/
    // 先停止消费者（解码线程），再停止生产者（解复用线程）

    if (video_decode_thread) video_decode_thread->Stop();  // 停止视频解码线程
    if (audio_decode_thread) audio_decode_thread->Stop();  // 停止音频解码线程
    if (demux_thread)        demux_thread->Stop();         // 停止解复用线程

    /*------------- 3. 删除音视频输出模块 -------------*/
    // 先停止输出模块，再删除对象

    delete audio_output;   // 删除音频输出模块（会关闭SDL音频设备）
    delete video_output;   // 删除视频输出模块（会关闭SDL窗口）
    audio_output = nullptr;
    video_output = nullptr;

    /*------------- 4. 清空队列 -------------*/
    // 清空帧队列（解码→输出）
    audio_frame_queue->Abort();  // 终止队列并释放所有帧
    video_frame_queue->Abort();

    // 清空包队列（解复用→解码）
    audio_packet_queue->Abort();  // 终止队列并释放所有包
    video_packet_queue->Abort();

    /*------------- 5. 删除线程对象 -------------*/
    delete audio_decode_thread;  // 删除音频解码线程对象
    delete video_decode_thread;  // 删除视频解码线程对象
    delete demux_thread;         // 删除解复用线程对象

    audio_decode_thread = nullptr;
    video_decode_thread = nullptr;
    demux_thread = nullptr;

    /*------------- 6. 重置播放状态 -------------*/
    started = false;  // 重置启动标志
    paused = false;   // 重置暂停标志
};
