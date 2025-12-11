#ifndef AVSYNC_H
#define AVSYNC_H

extern "C" {
#include <libavutil/time.h>    
}

#include <mutex>

/**
 * @brief 音视频同步类（AVSync）
 *
 * 功能：
 *   - 维护播放器主时钟（单位：秒）
 *   - 音频播放线程通过 SetClock 更新主时钟
 *   - 视频刷新线程通过 GetClock 获取当前时钟并决定是否显示帧
 *
 * 时钟原理：
 *   主时钟 = 系统时间 NowSec() + 偏移量 pts_drift_
 *   音频回调每次播放 PCM 时会调用 SetClock(pts)，驱动主时钟前进。
 */
class AVSync
{
public:
    AVSync() { InitClock(); };
    ~AVSync() {};

    /**
     * @brief 初始化主时钟，将其设置为 0 秒
     */
    void InitClock()
    {
        ResetClock(0.0);
    };

    /**
     * @brief 设置主时钟值（通常由音频线程调用）
     * @param pts 当前音频播放进度（单位：秒）
     *
     * 原理：
     *   偏移量 = 音频 pts - 当前系统时间
     */
    void SetClock(double pts)
    {
        std::lock_guard<std::mutex> lk(clock_mtx_);
        double now = NowSec();
        pts_drift_ = pts - now;
    };

    /**
     * @brief 重置主时钟为指定时间点（例如开始播放时）
     * @param pts 新的时钟起始值（秒）
     */
    void ResetClock(double pts)
    {
        std::lock_guard<std::mutex> lk(clock_mtx_);
        double now = NowSec();
        pts_drift_ = pts - now;
    };

    /**
     * @brief 获取当前主时钟值（视频线程用于同步）
     * @return 当前主时钟（秒）
     */
    double GetClock()
    {
        std::lock_guard<std::mutex> lk(clock_mtx_);
        double now = NowSec();

        return pts_drift_ + now;
    };

private:
    /**
     * @brief 获取当前系统相对时间（秒）
     */
    inline double NowSec() const
    {
        return av_gettime_relative() / 1000000.0;
    };

private:
    double pts_drift_ = 0.0;          // 主时钟偏移量（秒）
    mutable std::mutex clock_mtx_;    // 保护时钟的互斥锁
};

#endif // AVSYNC_H
