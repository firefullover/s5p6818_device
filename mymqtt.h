#ifndef MYMQTT_H
#define MYMQTT_H

#include <MQTTClient.h>
#include <stdlib.h>
#include <string.h>

// 不透明上下文类型
typedef struct MQTTContext MQTTContext;

// 初始化 MQTT 连接
MQTTContext* mqtt_init_default();

// 资源释放
void mqtt_close(MQTTContext **ctx);

// 消息发布接口
int mqtt_publish(MQTTContext *ctx, const char *topic, void *payload, int len);

// 设置订阅回调（需在主函数定义回调）
void mqtt_set_callback(MQTTContext *ctx, int (*msg_arrived)(void*, char*, int, MQTTClient_message*));

// 订阅主题
int mqtt_subscribe(MQTTContext *ctx, const char *topic);

#endif