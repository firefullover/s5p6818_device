#include "camera/camera_pub.h"

#define IMAGE_FILE "image.rgb"  // 临时测试文件，存储图像数据的文件路径

// 获取摄像头数据
void get_camera_data(unsigned char **buffer, long *size) {
    // 打开图像文件，以二进制方式读取
    FILE *file = fopen(IMAGE_FILE, "rb");
    if (!file) {
        perror("无法打开图像文件");
        *buffer = NULL;
        return;
    }

    // 将文件指针移动到文件末尾，获取文件的大小
    fseek(file, 0, SEEK_END);
    *size = ftell(file);
    rewind(file);

    // 校验文件大小是否为 240*240*2 字节
    const long required_size = 240 * 240 * 2;
    if (*size != required_size) {
        fprintf(stderr, "图像文件大小不正确，必须为 %ld 字节，实际为 %ld 字节\n", required_size, *size);
        fclose(file);
        *buffer = NULL;
        *size = 0;
        return;
    }

    // 为 buffer 分配足够的内存来存储图像数据
    *buffer = (unsigned char *)malloc(*size);
    if (!*buffer) {
        perror("内存分配失败");
        fclose(file);
        return;
    }

    // 从文件中读取图像数据到 buffer 中
    if (fread(*buffer, 1, *size, file) != *size) {
        fprintf(stderr, "图像读取不完整\n");
        free(*buffer);
        *buffer = NULL;
    }

    fclose(file);
}

