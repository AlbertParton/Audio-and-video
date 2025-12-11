cmake_minimum_required(VERSION 3.22.1)
project("androidplayer")

# FFmpeg 预编译动态库所在路径，jniLibs目录下按ABI分类
set(ffmpeg_lib_dir "${CMAKE_SOURCE_DIR}/../jniLibs/${ANDROID_ABI}")

# 项目的头文件目录（app/src/main/cpp/include）
set(project_include_dir "${CMAKE_SOURCE_DIR}/include")

# 查找Android系统日志库
find_library(log-lib log)

# 导入FFmpeg各个动态库
add_library(avcodec SHARED IMPORTED)
set_target_properties(avcodec PROPERTIES IMPORTED_LOCATION "${ffmpeg_lib_dir}/libavcodec.so")

add_library(avformat SHARED IMPORTED)
set_target_properties(avformat PROPERTIES IMPORTED_LOCATION "${ffmpeg_lib_dir}/libavformat.so")

add_library(avutil SHARED IMPORTED)
set_target_properties(avutil PROPERTIES IMPORTED_LOCATION "${ffmpeg_lib_dir}/libavutil.so")

add_library(swscale SHARED IMPORTED)
set_target_properties(swscale PROPERTIES IMPORTED_LOCATION "${ffmpeg_lib_dir}/libswscale.so")

add_library(swresample SHARED IMPORTED)
set_target_properties(swresample PROPERTIES IMPORTED_LOCATION "${ffmpeg_lib_dir}/libswresample.so")

add_library(avfilter SHARED IMPORTED)
set_target_properties(avfilter PROPERTIES IMPORTED_LOCATION "${ffmpeg_lib_dir}/libavfilter.so")

# 添加本地C++源文件
add_library(${PROJECT_NAME} SHARED
        AAudioRender.cpp
        ANWRender.cpp
        queue.cpp
        fqueue.cpp
        demuxer.cpp
        decoder.cpp
        render.cpp
        main_controller.cpp
)

# 添加头文件搜索路径
target_include_directories(${PROJECT_NAME} PRIVATE
        "${project_include_dir}"
)

# 链接系统库和FFmpeg库
target_link_libraries(${PROJECT_NAME}
        android
        avcodec
        avformat
        avutil
        swscale
        swresample
        aaudio
        ${log-lib}
)
