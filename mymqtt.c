#include "mymqtt.h"

// 内部默认配置参数（用于简化初始化）
#define DEFAULT_ADDRESS   "mqtt://192.168.5.109:1883"  // 默认 MQTT 服务器地址
#define DEFAULT_CLIENT_ID "s5p6818_Client"  // 默认客户端 ID
#define DEFAULT_QOS       1  // 默认服务质量等级（QoS 1）
#define DEFAULT_TIMEOUT   1000  // 默认超时时间（单位：毫秒）

// 定义 MQTT 上下文结构体（对外不透明）
struct MQTTContext {
    MQTTClient client;  // MQTT 客户端对象
};

// 使用默认参数初始化 MQTT 连接
MQTTContext* mqtt_init_default() {
    // 初始化 MQTT 连接选项
    MQTTClient_connectOptions conn_opts = MQTTClient_connectOptions_initializer;
    conn_opts.keepAliveInterval = 20;  // 设置心跳间隔 20 秒
    conn_opts.cleansession = 1;  // 设置清洁会话，断开连接后清除会话数据

    // 为 MQTT 上下文结构体分配内存
    MQTTContext *ctx = malloc(sizeof(MQTTContext));
    if (!ctx) return NULL;  // 内存分配失败则返回 NULL

    // 创建 MQTT 客户端实例
    MQTTClient_create(&ctx->client, DEFAULT_ADDRESS, DEFAULT_CLIENT_ID,
                     MQTTCLIENT_PERSISTENCE_NONE, NULL);
    
    // 连接到 MQTT 服务器
    if (MQTTClient_connect(ctx->client, &conn_opts) != MQTTCLIENT_SUCCESS) {
        free(ctx);  // 连接失败，释放已分配的内存
        return NULL;
    }

    return ctx;  // 返回已初始化的 MQTT 上下文
}

// 关闭 MQTT 连接并释放资源
void mqtt_close(MQTTContext **ctx) {
    if (*ctx) {
        // 断开 MQTT 连接，等待 10 秒以确保消息发送完成
        MQTTClient_disconnect((*ctx)->client, 10000);
        // 销毁 MQTT 客户端实例
        MQTTClient_destroy(&(*ctx)->client);
        // 释放 MQTT 上下文结构体内存
        free(*ctx);
        *ctx = NULL;  // 置空指针，避免野指针问题
    }
}

// 发布 MQTT 消息
int mqtt_publish(MQTTContext *ctx, const char *topic, void *payload, int len) {
    // 创建并初始化 MQTT 消息结构体
    MQTTClient_message pubmsg = {.payload = payload, .payloadlen = len, .qos = DEFAULT_QOS};

    // 定义消息投递令牌
    MQTTClient_deliveryToken token;

    // 发布消息到指定主题
    MQTTClient_publishMessage(ctx->client, topic, &pubmsg, &token);

    // 等待消息投递完成，并返回执行状态
    return MQTTClient_waitForCompletion(ctx->client, token, DEFAULT_TIMEOUT);
}

// 设置 MQTT 消息到达的回调函数
void mqtt_set_callback(MQTTContext *ctx, int (*msg_arrived)(void*, char*, int, MQTTClient_message*)) {
    MQTTClient_setCallbacks(ctx->client, NULL, NULL, msg_arrived, NULL);
}

// 订阅指定的 MQTT 主题
int mqtt_subscribe(MQTTContext *ctx, const char *topic) {
    return MQTTClient_subscribe(ctx->client, topic, DEFAULT_QOS);
}
