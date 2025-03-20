#include "engine.h"

// 舵机初始角度（90 度）
double eng2_deg = 90.0;
double eng3_deg = 90.0;

// 发送角度变化命令到舵机
void control_engine(int command, double *angle, double new_angle) {
    if (new_angle < 0 || new_angle > 180) {
        printf("角度超出范围 (0-180)，保持当前角度: %.1f 度\n", *angle);
        return;
    }

    int fd = open(ENGINE_DEVICE, O_RDWR);
    if (fd < 0) {
        perror("打开舵机设备失败");
        return;
    }

    double diff = new_angle - *angle;
    int steps = (int)(diff / DEG_UNIT);

    if (steps != 0) {
        ioctl(fd, steps, command);  // 发送控制命令
        *angle = new_angle;         // 更新角度
        printf("舵机 %d 角度调整为 %.1f 度\n", command, *angle);
    }

    close(fd);
}

// 舵机复位（将角度恢复到 90°）
void reset_engine() {
    printf("舵机复位中...\n");
    control_engine(Engine2, &eng2_deg, 90.0);
    control_engine(Engine3, &eng3_deg, 90.0);
    printf("舵机复位完成: eng2 = %.1f, eng3 = %.1f\n", eng2_deg, eng3_deg);
}

// 解析 JSON 并控制舵机
void parse_json_and_control(const char *json_data) {
    cJSON *root = cJSON_Parse(json_data);
    if (!root) {
        printf("JSON 解析失败\n");
        return;
    }

    cJSON *eng2_json = cJSON_GetObjectItem(root, "eng2");
    cJSON *eng3_json = cJSON_GetObjectItem(root, "eng3");

    if (cJSON_IsNumber(eng2_json) && cJSON_IsNumber(eng3_json)) {
        double new_eng2_deg = eng2_json->valuedouble;
        double new_eng3_deg = eng3_json->valuedouble;

        // 控制舵机
        control_engine(Engine2, &eng2_deg, new_eng2_deg);
        control_engine(Engine3, &eng3_deg, new_eng3_deg);
    } else {
        printf("JSON 数据格式错误\n");
    }

    cJSON_Delete(root);
}