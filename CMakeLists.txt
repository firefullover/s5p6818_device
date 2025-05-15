cmake_minimum_required(VERSION 3.10)
project(s5p6818_device_example)

# 包含头文件目录
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
    ${CMAKE_CURRENT_SOURCE_DIR}/../include/config
    ${CMAKE_CURRENT_SOURCE_DIR}/../include/camera
    ${CMAKE_CURRENT_SOURCE_DIR}/../include/engine
    ${CMAKE_CURRENT_SOURCE_DIR}/../include/mqtt
)

# 添加源文件
set(SRC_FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/main.c
    ${CMAKE_CURRENT_SOURCE_DIR}/camera/camera_pub.c
    ${CMAKE_CURRENT_SOURCE_DIR}/engine/engine.c
    ${CMAKE_CURRENT_SOURCE_DIR}/mqtt/mqtt.c
)

add_executable(s5p6818_device_example ${SRC_FILES})

# 链接 pthread 和 m（数学库）
target_link_libraries(s5p6818_device_example pthread m)