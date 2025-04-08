/* mqtt.h */
#ifndef MQTT_H
#define MQTT_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <MQTTClient.h>

// 配置常量
#define DEFAULT_ADDRESS   "tcp://192.168.5.109:1883" // 完整地址格式
#define DEFAULT_CLIENT_ID "s5p6818_Client"
#define DEFAULT_QOS       1
#define DEFAULT_TIMEOUT   1000
#define TOPIC_SUB         "6050_date"
#define TOPIC_PUB         "6818_image"

// 回调函数类型定义
typedef void (*message_handler)(char* topic, char* payload);

// MQTT 上下文结构体
typedef struct {
    MQTTClient client;
    message_handler handler;
} mqtt_ctx;

// 初始化MQTT连接
int mqtt_init(mqtt_ctx* ctx, message_handler handler);

// 发布消息
int mqtt_publish(mqtt_ctx* ctx, const char* topic, 
                const void* payload, size_t payload_len);

// 保持连接（需要在循环中调用）
void mqtt_loop(mqtt_ctx* ctx);

// 断开连接
void mqtt_disconnect(mqtt_ctx* ctx);

#endif