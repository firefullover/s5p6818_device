#include "camera_pub.h"

#define IMAGE_FILE "image.rgb"  // 临时测试文件，存储图像数据的文件路径

// 获取摄像头数据
void get_camera_data(unsigned char **buffer, long *size) {
    // 打开图像文件，以二进制方式读取
    FILE *file = fopen(IMAGE_FILE, "rb");
    if (!file) {
        // 文件打开失败时输出错误信息
        perror("无法打开图像文件");
        *buffer = NULL;  // 设置 buffer 为 NULL，表示失败
        return;
    }

    // 将文件指针移动到文件末尾，获取文件的大小
    fseek(file, 0, SEEK_END);
    *size = ftell(file);  // 获取文件大小
    rewind(file);  // 将文件指针重新指向文件开头

    // 为 buffer 分配足够的内存来存储图像数据
    *buffer = (unsigned char *)malloc(*size);
    if (!*buffer) {
        // 内存分配失败时输出错误信息
        perror("内存分配失败");
        fclose(file);  // 关闭文件
        return;
    }

    // 从文件中读取图像数据到 buffer 中
    if (fread(*buffer, 1, *size, file) != *size) {
        // 如果读取的字节数与文件大小不一致，表示读取不完整
        fprintf(stderr, "图像读取不完整\n");
        free(*buffer);  // 释放已分配的内存
        *buffer = NULL;  // 将 buffer 设置为 NULL，表示失败
    }

    // 关闭文件
    fclose(file);
}
