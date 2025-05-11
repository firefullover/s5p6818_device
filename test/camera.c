#include "camera.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <linux/videodev2.h>
#include <jpeglib.h>

#define MAX_BUFFERS 4  // 最大缓冲区数量

/* 摄像头上下文结构体 */
struct camera_ctx {
    int fd;                 // 设备文件描述符
    int width;              // 图像宽度
    int height;             // 图像高度
    enum pixel_format fmt;  // 像素格式
    
    struct {
        void* start[MAX_BUFFERS];   // 内存映射起始地址数组
        size_t length[MAX_BUFFERS]; // 每个缓冲区的长度
    } buffers;
    
    unsigned int n_buffers; // 实际分配的缓冲区数量
};

/*----------------------------------------------------------
 * V4L2设备初始化内部函数
 * 返回值：错误码
 *---------------------------------------------------------*/
static int init_v4l2_device(camera_ctx* ctx)
{
    // 查询设备能力
    struct v4l2_capability cap;
    if (ioctl(ctx->fd, VIDIOC_QUERYCAP, &cap) == -1) {
        return CAM_ERR_IOCTL;
    }

    // 检查是否支持视频采集
    if (!(cap.capabilities & V4L2_CAP_VIDEO_CAPTURE)) {
        return CAM_ERR_IOCTL;
    }

    // 设置视频格式
    struct v4l2_format fmt = {0};
    fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    fmt.fmt.pix.width = ctx->width;
    fmt.fmt.pix.height = ctx->height;
    fmt.fmt.pix.pixelformat = (ctx->fmt == FMT_JPEG) ? 
                             V4L2_PIX_FMT_JPEG : V4L2_PIX_FMT_RGB24;
    fmt.fmt.pix.field = V4L2_FIELD_NONE;

    if (ioctl(ctx->fd, VIDIOC_S_FMT, &fmt) == -1) {
        return CAM_ERR_IOCTL;
    }

    // 申请视频缓冲区
    struct v4l2_requestbuffers req = {0};
    req.count = MAX_BUFFERS;
    req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    req.memory = V4L2_MEMORY_MMAP;

    if (ioctl(ctx->fd, VIDIOC_REQBUFS, &req) == -1) {
        return CAM_ERR_IOCTL;
    }

    // 内存映射每个缓冲区
    for (ctx->n_buffers = 0; ctx->n_buffers < req.count; ++ctx->n_buffers) {
        struct v4l2_buffer buf = {0};
        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;
        buf.index = ctx->n_buffers;

        if (ioctl(ctx->fd, VIDIOC_QUERYBUF, &buf) == -1) {
            return CAM_ERR_IOCTL;
        }

        ctx->buffers.start[ctx->n_buffers] = mmap(
            NULL, 
            buf.length,
            PROT_READ | PROT_WRITE,
            MAP_SHARED,
            ctx->fd, 
            buf.m.offset
        );

        if (ctx->buffers.start[ctx->n_buffers] == MAP_FAILED) {
            return CAM_ERR_MMAP;
        }

        ctx->buffers.length[ctx->n_buffers] = buf.length;
    }

    // 将所有缓冲区加入队列
    for (unsigned int i = 0; i < ctx->n_buffers; ++i) {
        struct v4l2_buffer buf = {0};
        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;
        buf.index = i;

        if (ioctl(ctx->fd, VIDIOC_QBUF, &buf) == -1) {
            return CAM_ERR_IOCTL;
        }
    }

    return CAM_SUCCESS;
}

/*----------------------------------------------------------
 * 摄像头初始化
 *---------------------------------------------------------*/
camera_ctx* camera_init(const char* dev, 
                       int width, 
                       int height, 
                       enum pixel_format fmt,
                       int* error)
{
    camera_ctx* ctx = calloc(1, sizeof(*ctx));
    if (!ctx) {
        *error = CAM_ERR_OPEN;
        return NULL;
    }

    ctx->width = width;
    ctx->height = height;
    ctx->fmt = fmt;

    // 打开视频设备
    ctx->fd = open(dev, O_RDWR);
    if (ctx->fd < 0) {
        *error = CAM_ERR_OPEN;
        free(ctx);
        return NULL;
    }

    // 初始化V4L2设备
    int ret = init_v4l2_device(ctx);
    if (ret != CAM_SUCCESS) {
        *error = ret;
        close(ctx->fd);
        free(ctx);
        return NULL;
    }

    return ctx;
}

/*----------------------------------------------------------
 * 获取一帧图像（带超时机制）
 *---------------------------------------------------------*/
int camera_get_frame(camera_ctx* ctx, 
                    uint8_t** data, 
                    size_t* size, 
                    int timeout_ms)
{
    struct v4l2_buffer buf = {0};
    buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buf.memory = V4L2_MEMORY_MMAP;

    // 使用select实现超时控制
    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(ctx->fd, &fds);

    struct timeval tv = {
        .tv_sec = timeout_ms / 1000,
        .tv_usec = (timeout_ms % 1000) * 1000
    };

    int ret = select(ctx->fd + 1, &fds, NULL, NULL, &tv);
    if (ret <= 0) {
        return (ret == 0) ? -1 : CAM_ERR_IOCTL; // 超时或错误
    }

    // 从队列取出缓冲区
    if (ioctl(ctx->fd, VIDIOC_DQBUF, &buf) == -1) {
        return CAM_ERR_IOCTL;
    }

    *data = ctx->buffers.start[buf.index];
    *size = buf.bytesused;

    // 重新放回队列
    if (ioctl(ctx->fd, VIDIOC_QBUF, &buf) == -1) {
        return CAM_ERR_IOCTL;
    }

    return CAM_SUCCESS;
}

/*----------------------------------------------------------
 * JPEG解码核心逻辑
 *---------------------------------------------------------*/
int jpeg_to_rgb(const uint8_t* jpeg_data,
               size_t jpeg_size,
               uint8_t* rgb_buffer,
               int* width,
               int* height)
{
    struct jpeg_decompress_struct cinfo;
    struct jpeg_error_mgr jerr;

    // 初始化JPEG解压对象
    cinfo.err = jpeg_std_error(&jerr);
    jpeg_create_decompress(&cinfo);

    // 设置数据源为内存
    jpeg_mem_src(&cinfo, jpeg_data, jpeg_size);
    jpeg_read_header(&cinfo, TRUE);
    jpeg_start_decompress(&cinfo);

    *width = cinfo.output_width;
    *height = cinfo.output_height;

    // 检查是否为RGB格式
    if (cinfo.output_components != 3) {
        jpeg_destroy_decompress(&cinfo);
        return CAM_ERR_JPEG;
    }

    // 逐行读取扫描线
    while (cinfo.output_scanline < cinfo.output_height) {
        uint8_t* row = rgb_buffer + 
                      cinfo.output_scanline * cinfo.output_width * 3;
        jpeg_read_scanlines(&cinfo, &row, 1);
    }

    // 清理资源
    jpeg_finish_decompress(&cinfo);
    jpeg_destroy_decompress(&cinfo);
    return CAM_SUCCESS;
}

/*----------------------------------------------------------
 * 资源清理函数
 *---------------------------------------------------------*/
void camera_destroy(camera_ctx* ctx)
{
    if (ctx) {
        if (ctx->fd != -1) {
            // 停止视频流
            enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
            ioctl(ctx->fd, VIDIOC_STREAMOFF, &type);
            
            // 解除内存映射
            for (unsigned int i = 0; i < ctx->n_buffers; ++i) {
                if (ctx->buffers.start[i]) {
                    munmap(ctx->buffers.start[i], ctx->buffers.length[i]);
                }
            }
            close(ctx->fd);
        }
        free(ctx);
    }
}