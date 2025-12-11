#include <iostream>
#include <string>
#include <vector>
#include <map>
#include <thread>
#include <conio.h>
#include <windows.h>
#include "maincontroller.h"

extern "C" {
#include <libavutil/log.h>
}

using namespace std;

#undef main // 取消 SDL 对 main 的宏定义，避免冲突

// =======================
// 函数：扫描指定目录下的视频文件
// 参数：dir_path - 目录路径
// 返回值：视频文件名列表
// 支持格式：.mp4 .MP4 .mkv .avi
// =======================
vector<string> ScanVideoFiles(const string& dir_path) {
    vector<string> files;
    WIN32_FIND_DATAA find_data;  // Windows 文件查找数据结构

    // 构建搜索路径，使用通配符 *.* 匹配所有文件
    string search_path = dir_path + "\\*.*";
    HANDLE hFind = FindFirstFileA(search_path.c_str(), &find_data);

    // 如果查找失败，返回空向量
    if (hFind == INVALID_HANDLE_VALUE)
        return files;

    do {
        // 排除目录，只处理文件
        if (!(find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
            string name = find_data.cFileName;

            // 确保文件名至少4个字符（确保有扩展名）
            if (name.size() >= 4) {
                string ext = name.substr(name.size() - 4);  // 提取最后4个字符作为扩展名

                // 检查是否为支持的视频格式
                if (ext == ".mp4" || ext == ".MP4" || ext == ".mkv" || ext == ".avi") {
                    files.push_back(name);  // 添加到结果列表
                }
            }
        }
    } while (FindNextFileA(hFind, &find_data) != 0);  // 继续查找下一个文件

    FindClose(hFind);  // 关闭查找句柄

    return files;
};

// =======================
// 函数：选择视频文件
// 参数：video_dir - 视频目录路径
// 返回值：完整视频路径
// 说明：
//  1. 显示目录下视频列表，用户可通过字母编号或文件名（无后缀）选择视频
//  2. 按 Esc 键退出程序
// =======================
string SelectVideo(const string& video_dir) {
    // 扫描目录获取视频文件列表
    vector<string> video_files = ScanVideoFiles(video_dir);

    // 如果没有找到视频文件，提示并返回空字符串
    if (video_files.empty()) {
        cout << "未找到视频文件: " << video_dir << endl;
        return "";
    }

    // 建立选择映射表：用户输入 → 文件名
    // 支持两种输入方法：
    //  1. 字母编号：A, B, C...
    //  2. 文件名（不含扩展名）：video1, movie2...
    map<string, string> selection_map;
    char label = 'A';  // 从字母A开始编号

    cout << "\n视频列表:\n";  // 显示标题

    // 遍历所有视频文件，建立映射并显示列表
    for (auto& f : video_files) {
        cout << label << ". " << f << endl;  // 显示编号和文件名

        // 建立字母编号映射（如 "A" → "video1.mp4"）
        string key_upper(1, label);
        selection_map[key_upper] = f;

        // 建立文件名映射（不含扩展名）
        // 查找最后一个点号的位置，提取文件名（不含扩展名）
        string key_name = f.substr(0, f.find_last_of('.'));
        selection_map[key_name] = f;

        label++;  // 编号递增
    }

    string user_choice;  // 存储用户输入

    // 用户选择循环：持续等待直到用户做出有效选择
    while (true) {
        cout << "请选择视频（输入编号或文件名，无后缀），或按\"Esc键\"退出程序: ";
        user_choice.clear();  // 清空上一轮的选择

        // 逐字符读取用户输入
        while (true) {
            char ch = _getch();  // 无回显地获取单个字符

            // Esc键处理：直接退出整个程序
            if (ch == 27) {
                exit(0);
            }

            // 回车键处理：结束输入
            if (ch == '\r') {
                cout << endl;
                break;
            }

            // 退格键处理：删除最后一个字符
            if (ch == '\b') {
                if (!user_choice.empty()) {
                    user_choice.pop_back();  // 删除字符串最后一个字符
                    cout << "\b \b";  // 控制台光标回退并清除字符显示
                    // "\b" 光标回退一格，" " 输出空格覆盖原字符，再回退
                }
                continue;
            }

            // 只接受可打印字符（ASCII 32-126）
            if (ch >= 32 && ch <= 126) {
                user_choice += ch;  // 添加到用户选择字符串
                cout << ch;  // 回显字符，让用户看到输入
            }
        }

        // 输入为空时的错误处理
        if (user_choice.empty()) {
            cout << "输入不能为空，请重新输入！\n";
            continue;  // 重新开始选择循环
        }

        // 检查用户输入是否有效
        if (selection_map.find(user_choice) != selection_map.end()) {
            // 输入有效，构建完整文件路径并返回
            // 格式：目录路径 + 分隔符 + 文件名
            return video_dir + "\\" + selection_map[user_choice];
        }
        else {
            // 输入无效，提示用户重新输入
            cout << "无效选择，请重新输入！\n";
        }
    }

    return "";
};

// =======================
// 主函数
// =======================
int main()
{
    // 设置 FFmpeg 日志级别，只显示错误信息
    av_log_set_level(AV_LOG_ERROR);

    // 视频存放目录
    string video_dir = "./videos";

    // ===================== 外层循环：视频选择 =====================
    // 当用户选择退出当前视频（按E键）时，会回到这里选择新视频
    while (true) {
        // 调用 SelectVideo 函数，让用户选择要播放的视频
        string video_path = SelectVideo(video_dir);

        // 如果返回空字符串，表示没有视频或用户取消，退出程序
        if (video_path.empty())
            break;  // 退出外层循环，进而结束程序

        // ========== 创建主控制器，准备播放选中的视频 ==========
        // MainController 是整个播放器的核心，管理所有播放组件
        MainController controller(video_path.c_str());

        // ========== 显示播放器控制功能说明 ==========
        cout << "\n功能列表:\n";
        cout << "1.播放/继续播放：空格键\n";
        cout << "2.暂停：空格键\n";
        cout << "3.慢放：快捷键'S/s'，按一次切换至0.5倍速，再按一次回到1倍速\n";
        cout << "4.结束当前视频：快捷键'E/e'\n";
        cout << "5.退出程序：Esc键\n";

        // ===================== 内层循环：播放控制 =====================
        // 处理当前视频的播放控制，直到用户选择结束当前视频
        while (true) {
            // _kbhit() 检查是否有键盘按键被按下（非阻塞）
            if (_kbhit()) {
                int ch = _getch();  // 获取按下的键

                // ------------ 空格键：播放 / 暂停 / 恢复 ------------
                if (ch == 32) {
                    // 逻辑：
                    // 1. 如果未开始播放 → 开始播放
                    // 2. 如果正在播放 → 暂停
                    // 3. 如果已暂停 → 恢复播放
                    if (!controller.isStarted())
                        controller.start();   // 首次启动播放
                    else if (!controller.isPaused())
                        controller.pause();   // 播放中 → 暂停
                    else
                        controller.resume();  // 已暂停 → 恢复播放
                }
                // ------------ E/e：结束当前视频 ------------
                else if (ch == 'e' || ch == 'E') {
                    controller.stop();  // 停止播放并清理资源
                    break;  // 退出内层循环，返回视频选择界面
                }
                // ------------ Esc键：退出程序 ------------
                else if (ch == 27) {
                    // 如果正在播放，先停止播放器
                    if (controller.isStarted())
                        controller.stop();  // 清理播放资源
                    return 0;  // 直接退出程序
                }
                // ------------ S/s：倍速切换 ------------
                else if (ch == 's' || ch == 'S') {
                    // 获取当前倍速，在0.5x和1.0x之间切换
                    float current_speed = controller.getSpeed();
                    float new_speed = (current_speed == 1.0f ? 0.5f : 1.0f);

                    // 设置新的播放速度
                    controller.setSpeed(new_speed);
                    cout << "当前倍速：" << new_speed << "x\n";  // 显示提示信息
                }
            }

            // 如果没有按键输入，休眠10毫秒，减少CPU占用
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }

    return 0;
};

