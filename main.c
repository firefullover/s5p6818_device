#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <MQTTClient.h>
#include "engine.h"
#include <pthread.h>

#define ADDRESS   "tcp://broker.hivemq.com:1883"  // MQTT 服务器地址
#define CLIENTID  "C_Client"  // 客户端 ID
#define TOPIC_SUB "6050_date"  // 订阅主题
#define TOPIC_PUB "6818_image"  // 发布主题
#define QOS       1  // 服务质量等级
#define TIMEOUT   1000L  // 超时时间
#define IMAGE_FILE "image.rgb"  // 240x240 16位RGB图像文件路径

volatile int finished = 0;  // 控制主循环的变量

// 线程函数，负责解析 JSON 并控制舵机
void *json_parse_thread(void *arg) {
    char *json_data = (char *)arg;
    parse_json_and_control(json_data);  // 解析 JSON 并控制舵机
    free(json_data);  // 释放动态分配的内存
    return NULL;
}

// 订阅消息回调函数（实时监听）// MQTT 订阅回调函数
int messageArrived(void *context, char *topicName, int topicLen, MQTTClient_message *message) {
    // 检查是否是目标主题
    if (strcmp(topicName, TOPIC_SUB) != 0) {
        printf("收到无关主题 %s 的消息，忽略。\n", topicName);
        MQTTClient_freeMessage(&message);
        MQTTClient_free(topicName);
        return 1;
    }

    printf("收到来自主题 %s 的消息:\n", topicName);
    printf("消息内容: %.*s\n", message->payloadlen, (char *)message->payload);

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
}

// 读取 240x240 16 位 RGB 图像并发布到 MQTT
void publish_image(MQTTClient *client) {

    FILE *file = fopen(IMAGE_FILE, "rb");
    if (!file) {
        perror("无法打开图像文件");
        return;
    }

    fseek(file, 0, SEEK_END);
    long fileSize = ftell(file);
    rewind(file);

    char *buffer = (char *)malloc(fileSize);
    if (!buffer) {
        perror("内存分配失败");
        fclose(file);
        return;
    }

    fread(buffer, 1, fileSize, file);
    fclose(file);

    MQTTClient_message pubmsg = {0};
    pubmsg.payload = buffer;
    pubmsg.payloadlen = fileSize;
    pubmsg.qos = QOS;
    pubmsg.retained = 0;

    MQTTClient_deliveryToken token;
    MQTTClient_publishMessage(*client, TOPIC_PUB, &pubmsg, &token);
    MQTTClient_waitForCompletion(*client, token, TIMEOUT);
    printf("成功发布图像文件 (%ld 字节) 到主题: %s\n", fileSize, TOPIC_PUB);

    free(buffer);
}

int main() {
    reset_engine();  // 初始化引擎

    MQTTClient client;
    MQTTClient_connectOptions conn_opts = MQTTClient_connectOptions_initializer;
    conn_opts.keepAliveInterval = 20;
    conn_opts.cleansession = 1;

    // 创建 MQTT 客户端
    MQTTClient_create(&client, ADDRESS, CLIENTID, MQTTCLIENT_PERSISTENCE_NONE, NULL);
    MQTTClient_setCallbacks(client, NULL, NULL, messageArrived, NULL);

    // 连接到 MQTT 服务器
    if (MQTTClient_connect(client, &conn_opts) != MQTTCLIENT_SUCCESS) {
        fprintf(stderr, "连接到 MQTT 服务器失败\n");
        return EXIT_FAILURE;
    }

    // 订阅 6050_date 主题（实时监听）
    MQTTClient_subscribe(client, TOPIC_SUB, QOS);
    printf("已订阅主题: %s\n", TOPIC_SUB);

    // 进入阻塞循环，保持客户端运行，实时监听数据
    while (!finished) {
        MQTTClient_yield();  // 保持 MQTT 客户端活跃，实时接收消息
    }

    // 断开 MQTT 连接
    MQTTClient_disconnect(client, 10000);
    MQTTClient_destroy(&client);
    return EXIT_SUCCESS;
}
