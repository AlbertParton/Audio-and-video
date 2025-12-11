#ifndef MAIN_CONTROLLER_H
#define MAIN_CONTROLLER_H

#include <android/native_window.h>

extern "C" {
#include <libavformat/avformat.h>
}

class Decoder;
class Render;

class MainController {
public:
    MainController();

    ~MainController();

    void run(const char *inputPath, const char *outputYUVPath);
    void run(const char *inputPath,  ANativeWindow *window);

    // 暂停和恢复
    void pause();
    void resume();

private:
    Render* render_ = nullptr;
    static MainController *currentInstance;
};

#endif // MAIN_CONTROLLER_H
