/* mqtt.h */
#ifndef MQTT_H
#define MQTT_H

#include <stdio.h>
#include <stdint.h>
#include <config/config.h>
#include <MQTTClient.h>

// 回调函数类型定义
typedef void (*message_handler)(const char* payload);

// mqtt视频帧头部
typedef struct {
    uint32_t frame_id;// 帧ID
    uint32_t frame_len;// payload长度
} __attribute__((packed)) frame_header_t;

// MQTT 上下文结构体
typedef struct {
    MQTTClient client;
    message_handler handler;
    int connected;                // 连接状态标志
    unsigned long last_reconnect; // 上次重连尝试时间（毫秒时间戳）
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