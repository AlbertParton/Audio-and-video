#include "main_controller.h"
#include "demuxer.h"
#include "decoder.h"
#include "render.h"
#include "queue.h"
#include "fqueue.h"

#include <android/log.h>
#include <jni.h>
#include <thread>
#include <chrono>
#include <android/native_window_jni.h>

#define TAG "MainController"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, TAG, __VA_ARGS__)

// 初始化静态成员
MainController* MainController::currentInstance = nullptr;

MainController::MainController() {}

MainController::~MainController()
{
    currentInstance = nullptr;
}

void MainController::run(const char *inputPath, const char *outputYUVPath) {
    av_log_set_level(AV_LOG_INFO);
    avformat_network_init();

    PacketQueue *videoQueue = new PacketQueue();
    Demuxer *demuxer = new Demuxer();
    Decoder *decoder = new Decoder();

    AVCodecParameters *videoParams = nullptr;

    demuxer->open(inputPath);
    demuxer->setQueue(videoQueue);
    videoParams = demuxer->getVideoCodecParams();

    decoder->init(videoParams);
    decoder->setQueue(videoQueue);
    decoder->setOutputFile(outputYUVPath);

    demuxer->start();
    decoder->start();

    // 等待解复用线程结束并且队列清空
    while (!demuxer->isFinished() || !videoQueue->isEmpty()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10)); // 替换av_usleep
    }

    LOGI("解复用完成，等待解码器退出...");
    demuxer->stop();
    decoder->stop();

    cleanup:
    delete demuxer;
    delete decoder;
    delete videoQueue;

    avformat_network_deinit();

    LOGI("处理完成，YUV已保存：%s", outputYUVPath);
}

void MainController::run(const char *inputPath,  ANativeWindow *window) {
    av_log_set_level(AV_LOG_INFO);
    avformat_network_init();

    PacketQueue *videoQueue = new PacketQueue();
    FrameQueue *frameQueue = new FrameQueue();
    Demuxer *demuxer = new Demuxer();
    Decoder *decoder = new Decoder();
    render_ = new Render();

    AVCodecParameters *videoParams = nullptr;

    demuxer->open(inputPath);
    demuxer->setQueue(videoQueue);
    videoParams = demuxer->getVideoCodecParams();

    decoder->init(videoParams);

    render_ ->init(decoder->getWidth(), decoder->getHeight(), window);

    decoder->setQueue(videoQueue);
    decoder->setFrameQueue(frameQueue);
    render_ ->setFrameQueue(frameQueue);

    demuxer->start();
    decoder->start();
    render_ ->start();

    // 等待解复用线程结束并且队列清空
    while (!demuxer->isFinished() || !videoQueue->isEmpty()|| !frameQueue->isEmpty()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10)); // 替换av_usleep
    }

    LOGI("解复用完成，等待解码器和渲染器退出...");
    demuxer->stop();
    decoder ->stop();
    render_ ->stop();

    cleanup:
    delete demuxer;
    delete decoder ;
    delete render_ ;
    delete videoQueue;
    delete frameQueue;

    avformat_network_deinit();
}

void MainController::pause() {
    if (render_) {
        render_->pause();
    }

    LOGI("播放已暂停");
}

void MainController::resume() {
    if (render_) {
        render_->resume();
    }

    LOGI("播放已恢复");
}

extern "C"
JNIEXPORT void JNICALL
Java_com_example_androidplayer_Player_nativeRunDecode(JNIEnv *env, jobject /*thiz*/, jstring input,
                                                      jstring output ) {
    if (input == nullptr || output == nullptr) {
        LOGE("nativeRunDecode 输入参数为空");
        return;
    }

    const char *inputPath = env->GetStringUTFChars(input, nullptr);
    const char *outputPath = env->GetStringUTFChars(output, nullptr);

    if (inputPath == nullptr || outputPath == nullptr) {
        LOGE("GetStringUTFChars 失败");
        if (inputPath) env->ReleaseStringUTFChars(input, inputPath);
        if (outputPath) env->ReleaseStringUTFChars(output, outputPath);
        return;
    }

    MainController controller;
    controller.run(inputPath, outputPath);

    env->ReleaseStringUTFChars(input, inputPath);
    env->ReleaseStringUTFChars(output, outputPath);
}

extern "C"
JNIEXPORT jint JNICALL
Java_com_example_androidplayer_Player_nativePlay(JNIEnv *env, jobject thiz, jstring file,
                                                 jobject surface) {
    if (file == nullptr || surface == nullptr) {
        LOGE("nativePlay: input or surface is null");
        return -1;
    }

    const char *inputPath = env->GetStringUTFChars(file, nullptr);
    if (inputPath == nullptr) {
        LOGE("nativePlay: GetStringUTFChars failed for input");
        return -1;
    }

    // 从Java Surface对象获取ANativeWindow
    ANativeWindow *window = ANativeWindow_fromSurface(env, surface);
    if (window == nullptr) {
        LOGE("nativePlay: ANativeWindow_fromSurface failed");
        env->ReleaseStringUTFChars(file, inputPath);
        return -1;
    }

    // 创建控制器并运行播放流程
    MainController controller;
    controller.run(inputPath,  window);

    // 清理资源
    env->ReleaseStringUTFChars(file, inputPath);
    ANativeWindow_release(window);

    return 0;
}




