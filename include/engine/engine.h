#ifndef ENGINE_H
#define ENGINE_H

#include <stdio.h>
#include <stdlib.h>
#include <config/config.h>

// 舵机编号定义
#define Engine2 0x1
#define Engine3 0x2

// 全局变量声明
extern double eng2_deg;
extern double eng3_deg;

// 定义舵机控制参数结构体
// struct engine_angle {
//     int command;  // 控制命令（Engine2/Engine3）
//     int steps;    // 转动步数（基于DEG_UNIT计算）
// };

void handle_angle_control(const char *json_data);
int engine_init();
void print_engine_angle();
void parse_json_and_control(const char *json_data);
void reset_engine();
void control_engine(int command, double *angle, double new_angle);
void engine_close();

#endif