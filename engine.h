#ifndef ENGINE_H
#define ENGINE_H

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <string.h>
#include <cjson/cJSON.h>

// 舵机设备文件
#define ENGINE_DEVICE "/dev/myengine"

// 舵机控制命令
#define Engine2 0x1  // 左右转动
#define Engine3 0x2  // 上下转动
#define DEG_UNIT 1.8 // 舵机最小转动单位
#define DELAY_MS 50  // 控制延时

// 舵机角度
extern double eng2_deg;
extern double eng3_deg;

// 解析 JSON 数据并更新舵机状态
void parse_json_and_control(const char *json_data);
void reset_engine();
void control_engine(int command, double *angle, double new_angle);

#endif // ENGINE_H
