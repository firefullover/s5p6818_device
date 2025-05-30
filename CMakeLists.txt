cmake_minimum_required(VERSION 3.10)

# 定义编译器
if(APPLE)# MacOS
    set(CMAKE_CXX_COMPILER clang)
elseif(UNIX)# Linux
    set(CMAKE_CXX_COMPILER gcc)
elseif(WIN32)# Windows
    set(CMAKE_CXX_COMPILER cl)
endif()

project(s5p6818_device_example)

# 查找FFmpeg库（包括avdevice）
find_package(PkgConfig REQUIRED)
pkg_check_modules(FFMPEG REQUIRED libavformat libavcodec libavutil libswscale libavdevice)

# 包含头文件目录
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${FFMPEG_INCLUDE_DIRS}  # 添加FFmpeg头文件目录
)

# 添加源文件
set(SRC_FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/src/main.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/camera/camera_test.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/engine/engine.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/mqtt/mqtt.c
)

# 生成可执行文件
add_executable(s5p6818_device_example ${SRC_FILES})

# 链接
target_link_libraries(s5p6818_device_example 
    pthread 
    m
    jpeg
    cjson
    paho-mqtt3c
    ${FFMPEG_LIBRARIES}  # 添加FFmpeg库
)

# 设置编译选项
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${FFMPEG_CFLAGS_OTHER}")