#include "engine.h"

// 舵机初始角度定义
double eng2_deg = 90.0;
double eng3_deg = 90.0;
static int engine_fd = -1;  // 设备文件描述符

// 初始化舵机设备
void engine_init() {
    engine_fd = open(ENGINE_DEVICE, O_RDWR);
    if (engine_fd < 0) {
        perror("舵机设备初始化失败");
        exit(1); // 退出程序
    }
    printf("舵机设备打开\n");
}

// 打印舵机角度
void print_engine_angle() {
    printf("当前角度 eng2=%.1f, eng3=%.1f\n", eng2_deg, eng3_deg);
}

// 解析 JSON 数据并控制舵机
void parse_json_and_control(const char *json_data) {
    char *angle_y_pos = strstr(json_data, "\"angle_y\":");
    char *angle_z_pos = strstr(json_data, "\"angle_z\":");

    if (angle_y_pos && angle_z_pos) {
        double new_eng2_deg = 0, new_eng3_deg = 0;

        if (sscanf(angle_y_pos, "\"angle_y\":%lf", &new_eng2_deg) == 1 &&
            sscanf(angle_z_pos, "\"angle_z\":%lf", &new_eng3_deg) == 1) {

            // 控制舵机
            control_engine(Engine2, &eng2_deg, new_eng2_deg);
            control_engine(Engine3, &eng3_deg, new_eng3_deg);
        } else {
            printf("JSON 数据格式错误\n");
        }
    } else {
        printf("JSON 解析失败: 关键字段缺失\n");
    }
}

// 复位到初始角度
void reset_engine() {
    printf("开始复位舵机...\n");
    control_engine(Engine2, &eng2_deg, 90.0);
    control_engine(Engine3, &eng3_deg, 90.0);
    printf("复位完成: 当前角度 eng2=%.1f, eng3=%.1f\n", eng2_deg, eng3_deg);
}

// 控制舵机核心逻辑
void control_engine(int command,double *angle ,double new_angle) {
    // 计算角度的PWM占比
    int steps = (int)round(new_angle / DEG_UNIT);  // 使用更精确的round
    if (steps != 0) {
        // 构造ioctl参数
        
        if (ioctl(engine_fd,steps,command) < 0) {  // 使用新命令
            perror("舵机控制失败");
        } else {
            *angle = new_angle;  // 更新角度
            printf("舵机 %d 已调整到 %.1f 度 \n", command, *angle);
        }
    }
}

void engine_close() {
    if (engine_fd >= 0) {
        close(engine_fd);
        engine_fd = -1;
        printf("舵机设备已关闭\n");
    }
}