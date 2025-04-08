/* camera.h */
#ifndef CAMERA_H
#define CAMERA_H

#include <stdint.h>
#include <stddef.h>

// 错误码定义
#define CAM_SUCCESS 0
#define CAM_ERR_OPEN -1
#define CAM_ERR_IOCTL -2
#define CAM_ERR_MMAP -3
#define CAM_ERR_JPEG -4

// 像素格式
enum pixel_format {
    FMT_JPEG = 0,
    FMT_RGB24
};

// 摄像头上下文句柄
typedef struct camera_ctx camera_ctx;

// 初始化摄像头
camera_ctx* camera_init(const char* dev, 
                        int width, 
                        int height, 
                        enum pixel_format fmt,
                        int* error);

// 开始视频流捕获
int camera_start_capture(camera_ctx* ctx);

// 获取一帧数据 (需手动释放返回的data)
int camera_get_frame(camera_ctx* ctx, 
                    uint8_t** data, 
                    size_t* size, 
                    int timeout_ms);

// 释放帧数据
void camera_release_frame(uint8_t* data);

// 停止捕获
void camera_stop_capture(camera_ctx* ctx);

// 清理资源
void camera_destroy(camera_ctx* ctx);

// JPEG解码为RGB24 (需预分配rgb_buffer空间)
int jpeg_to_rgb(const uint8_t* jpeg_data,
               size_t jpeg_size,
               uint8_t* rgb_buffer,
               int* width,
               int* height);

#endif