#ifndef CAMERA_TEST_H
#define CAMERA_TEST_H

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <config/config.h>

// 摄像头配置结构体
typedef struct {
    char *device;           // 摄像头设备路径，如 "/dev/video0"
    int width;              // 输出图像宽度
    int height;             // 输出图像高度
    int fps;                // 目标帧率
    bool is_initialized;    // 初始化状态标志
} camera_config_t;

// 初始化摄像头，设置参数并打开设备
int camera_init(camera_config_t *config);

/**
 * @brief 从摄像头获取一帧图像，转换为240*240*16位RGB格式
 * @param buffer 输出参数，函数内部会分配内存，调用者负责释放
 * @param size 输出参数，返回buffer的大小
 * @return int 0-成功，负数-失败
 */
int camera_get_frame(unsigned char **buffer, long *size);

// 关闭摄像头，释放资源
void camera_deinit(void);

// 从文件读取图像数据（测试函数）
void get_image_data(unsigned char **buffer, long *size);

#endif