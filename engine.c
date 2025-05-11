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
        return;
    }
    printf("舵机设备打开\n");
}

// 打印舵机角度
void print_engine_angle() {
    printf("当前角度 eng2=%.1f, eng3=%.1f\n", eng2_deg, eng3_deg);
}

// 解析 JSON 数据并控制舵机
void parse_json_and_control(const char *json_data) {
    if (!json_data) {
        printf("JSON 数据为空\n");
        return;
    }
    
    // 命令类型检查
    char *cmd_type = strstr(json_data, "\"cmd_type\":");
    char cmd_value[32] = {0};
    
    // 如果存在命令类型字段，则解析命令类型
    // {"cmd_type":"angle_control","angle_y":45.0,"angle_z":-30.0}
    if (cmd_type && sscanf(cmd_type, "\"cmd_type\":\"%31[^\"]\"", &cmd_value) == 1) {
        // 处理不同类型的命令
        if (strcmp(cmd_value, "angle_control") == 0) {
            // 角度控制命令
            handle_angle_control(json_data);
        } else if (strcmp(cmd_value, "reset") == 0) {
            // 复位命令
            reset_engine();
        } else if (strcmp(cmd_value, "status") == 0) {
            // 状态查询命令
            printf("当前舵机状态: Engine2=%.2f度, Engine3=%.2f度\n", eng2_deg, eng3_deg);
        } else {
            printf("未知命令类型: %s\n", cmd_value);
        }
    } else {
        // 向后兼容：如果没有命令类型，则按照旧格式处理
        handle_angle_control(json_data);
    }
}

// 处理角度控制命令
void handle_angle_control(const char *json_data) {
    char *angle_y_pos = strstr(json_data, "\"angle_y\":");
    char *angle_z_pos = strstr(json_data, "\"angle_z\":");
    double new_eng2_deg = 0, new_eng3_deg = 0;
    int changes = 0;
    
    // 解析Y轴角度（Engine2）
    if (angle_y_pos && sscanf(angle_y_pos, "\"angle_y\":%lf", &new_eng2_deg) == 1) {
        // 检查角度范围
        if (new_eng2_deg >= -90.0 && new_eng2_deg <= 90.0) {
            control_engine(Engine2, &eng2_deg, new_eng2_deg);
            changes++;
        } else {
            printf("Y轴角度超出范围 [-90,90]: %.2f\n", new_eng2_deg);
        }
    }
    
    // 解析Z轴角度（Engine3）
    if (angle_z_pos && sscanf(angle_z_pos, "\"angle_z\":%lf", &new_eng3_deg) == 1) {
        // 检查角度范围
        if (new_eng3_deg >= -90.0 && new_eng3_deg <= 90.0) {
            control_engine(Engine3, &eng3_deg, new_eng3_deg);
            changes++;
        } else {
            printf("Z轴角度超出范围 [-90,90]: %.2f\n", new_eng3_deg);
        }
    }
    
    if (changes == 0) {
        printf("JSON 解析失败: 未找到有效的角度控制参数\n");
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