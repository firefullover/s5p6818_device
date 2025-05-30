#include <engine/engine.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <string.h>
#include <cjson/cJSON.h>
#include <math.h>

// 舵机初始角度定义
double eng2_deg = 90.0;
double eng3_deg = 90.0;
static int engine_fd = -1;  // 设备文件描述符

// 初始化舵机设备
int engine_init() {
    engine_fd = open(ENGINE_DEVICE, O_RDWR);
    if (engine_fd < 0) {
        perror("舵机设备初始化失败");
        return -1;
    }
    reset_engine();
    return 0;
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

    cJSON *root = cJSON_Parse(json_data);
    if (!root) {
        printf("JSON 解析失败\n");
        return;
    }

    /**
     * json_data 格式:
     * {
     *   "cmd_type": "angle_control",
     *   "angle_y": 45.0,
     *   "angle_z": 45.0
     * }
     *
     * 或者
     * {
     *   "cmd_type": "reset"
     * }
     */

    cJSON *cmd_type_obj = cJSON_GetObjectItem(root, "cmd_type");
    if (cmd_type_obj && cJSON_IsString(cmd_type_obj)) {
        const char *cmd_type = cmd_type_obj->valuestring;
        if (strcmp(cmd_type, "angle_control") == 0) {
            handle_angle_control(json_data); // 直接传递原始数据，内部也要用cJSON
        } else if (strcmp(cmd_type, "reset") == 0) {
            reset_engine();
        } else if (strcmp(cmd_type, "status") == 0) {
            printf("当前舵机状态: Engine2=%.2f度, Engine3=%.2f度\n", eng2_deg, eng3_deg);
        } else {
            printf("未知命令类型: %s\n", cmd_type);
        }
    } else {
        // 向后兼容：如果没有命令类型，则按照旧格式处理
        handle_angle_control(json_data);
    }
    cJSON_Delete(root);
}

void handle_angle_control(const char *json_data) {
    cJSON *root = cJSON_Parse(json_data);
    if (!root) {
        printf("JSON 解析失败: 非法JSON\n");
        return;
    }

    cJSON *angle_y_obj = cJSON_GetObjectItem(root, "angle_y");
    cJSON *angle_z_obj = cJSON_GetObjectItem(root, "angle_z");
    double new_eng2_deg = 0, new_eng3_deg = 0;
    int changes = 0;

    if (angle_y_obj && cJSON_IsNumber(angle_y_obj)) {
        new_eng2_deg = angle_y_obj->valuedouble;
        if (new_eng2_deg >= -90.0 && new_eng2_deg <= 90.0) {
            control_engine(Engine2, &eng2_deg, new_eng2_deg);
            changes++;
        } else {
            printf("Y轴角度超出范围 [-90,90]: %.2f\n", new_eng2_deg);
        }
    }

    if (angle_z_obj && cJSON_IsNumber(angle_z_obj)) {
        new_eng3_deg = angle_z_obj->valuedouble;
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
    cJSON_Delete(root);
}

// 复位到初始角度
void reset_engine() {
    printf("开始复位舵机...\n");
    control_engine(Engine2, &eng2_deg, 90.0);
    control_engine(Engine3, &eng3_deg, 90.0);
    printf("复位完成: 当前角度 eng2=%.1f, eng3=%.1f\n", eng2_deg, eng3_deg);
}

// 控制舵机核心逻辑
void control_engine(int command, double *angle, double new_angle) {
    if (engine_fd < 0) {
        printf("舵机设备未初始化，无法控制舵机。\n");
        return;
    }
    int steps = (int)round(new_angle / DEG_UNIT);  // 使用更精确的round
    if (steps != 0) {
        // 构造ioctl参数
        if (ioctl(engine_fd, steps, command) < 0) {  // 使用新命令
            perror("舵机控制失败");
        } else {
            *angle = new_angle;  // 更新角度
            printf("舵机 %d 已调整到 %.1f 度 \n", command, *angle);
        }
    }
}

void engine_close() {
    if (engine_fd >= 0) {
        reset_engine();
        close(engine_fd);
        engine_fd = -1;
        printf("舵机设备已关闭\n");
    }
}