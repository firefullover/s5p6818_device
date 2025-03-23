#ifndef ENGINE_H
#define ENGINE_H

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <string.h>
#include <pthread.h>  // 添加线程支持
#include <math.h>     // 添加数学函数支持

#define ENGINE_DEVICE "/dev/myengine"
// 角度单位与延迟
#define Engine2 0x1
#define Engine3 0x2
#define DEG_UNIT 1.8
#define DELAY_MS 50

// 修改后的 ioctl 命令定义（方向+步进数）
#define ENGINE_SET_ANGLE _IOW('E', 1, struct engine_angle)  // 使用结构体传递参数

// 全局变量声明
extern double eng2_deg;
extern double eng3_deg;
extern pthread_mutex_t engine_mutex;  // 互斥锁

// 定义舵机控制参数结构体
struct engine_angle {
    int command;  // 控制命令（Engine2/Engine3）
    int steps;    // 转动步数（基于DEG_UNIT计算）
};

// 函数声明
void engine_init();
void parse_json_and_control(const char *json_data);
void reset_engine();
void control_engine(int command, double *angle, double new_angle);
void engine_close();

#endif // ENGINE_H