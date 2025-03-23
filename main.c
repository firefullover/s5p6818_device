#include <stdio.h>
#include <signal.h>
#include "engine.h"
#include "camera_pub.h"

#define TOPIC_SUB "6050_date"  // 订阅主题
#define TOPIC_PUB "6818_image"  // 发布主题

volatile int finished = 0;  // 控制主循环的变量

void sig_handler(int sig) {
    printf("\n收到终止信号，清理资源...\n");
    finished = 1;
}

// 线程函数，负责解析 JSON 并控制舵机
void *json_parse_thread(void *arg) {
    char *json_data = (char *)arg;
 
    parse_json_and_control(json_data);
    
    free(json_data);
    return NULL;
}

// MQTT 订阅回调函数
int messageArrived(void *context, char *topicName, int topicLen, MQTTClient_message *message) {
    // 检查是否是目标主题
    if (strcmp(topicName, TOPIC_SUB) != 0) {
        printf("收到无关主题 %s 的消息，忽略。\n", topicName);
        MQTTClient_freeMessage(&message);
        MQTTClient_free(topicName);
        return -1;
    }

    // printf("收到来自主题 %s 的消息:\n", topicName);
    // printf("消息内容: %.*s\n", message->payloadlen, (char *)message->payload);

    // 复制 JSON 数据，确保线程能安全使用
    char *json_data = malloc(message->payloadlen + 1);
    if (json_data) {
        memcpy(json_data, message->payload, message->payloadlen);
        json_data[message->payloadlen] = '\0';  // 确保字符串终止
    }

    // 创建新线程处理 JSON 解析和舵机控制
    pthread_t thread;
    if (pthread_create(&thread, NULL, json_parse_thread, json_data) != 0) {
        perror("线程创建失败");
        free(json_data);
    } else {
        pthread_detach(thread);  // 线程自动回收资源
    }

    MQTTClient_freeMessage(&message);
    MQTTClient_free(topicName);

	return 0;
}

int main() {
    signal(SIGINT, sig_handler);
    engine_init();
    reset_engine();

    // 初始化 MQTT
    MQTTContext *mqtt_ctx = mqtt_init_default();
    if (!mqtt_ctx) {
        fprintf(stderr, "MQTT 连接失败\n");
        engine_close();
        return 0;
    }

    // 设置回调并订阅
    mqtt_set_callback(mqtt_ctx, messageArrived);
    mqtt_subscribe(mqtt_ctx, TOPIC_SUB);

    // 主循环（示例每秒发布图像）
    while (!finished) {
        MQTTClient_yield();  // 保持 MQTT 活跃

        unsigned char *img_data;
        long img_size;
        get_camera_data(&img_data, &img_size);
        if (img_data) {
            mqtt_publish(mqtt_ctx, TOPIC_PUB, img_data, img_size);
            free(img_data);
        }
        sleep(1);
    }

    // 资源清理
    engine_close();
    mqtt_close(&mqtt_ctx);
    return 1;
}