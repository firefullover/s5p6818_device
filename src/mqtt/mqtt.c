#include <mqtt/mqtt.h>
#include <engine/engine.h>  // 包含舵机控制头文件
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <sys/time.h>


// MQTT 消息送达回调函数
static void delivered(void *context, MQTTClient_deliveryToken dt) {
    printf("消息已发布: %d\n", dt);
}

// MQTT 消息收到回调函数
static int msgarrvd(void *context, char *topicName, int topicLen, 
                   MQTTClient_message *message) {
    mqtt_ctx* ctx = (mqtt_ctx*)context; // 获取上下文指针
    char* payload = NULL;

    // 安全检查，确保消息和载荷有效
    if (message && message->payload && message->payloadlen > 0) {
        // 为消息载荷分配内存，并拷贝内容，确保以 '\0' 结尾
        payload = malloc(message->payloadlen + 1);
        if (payload) {
            memcpy(payload, message->payload, message->payloadlen);
            payload[message->payloadlen] = '\0';

            // 如果主题名等于 TOPIC_SUB，则打印消息内容并调用回调处理数据
            if(topicName && strcmp(topicName, TOPIC_SUB) == 0) {
                if(ctx->handler) {
                    ctx->handler(payload);// 调用舵机库的解析函数
                }
            }
        } else {
            fprintf(stderr, "内存分配失败\n");
        }
    } else {
        fprintf(stderr, "收到无效消息\n");
    }

    // 释放分配的内存，防止内存泄漏
    if (payload) free(payload);
    // 释放 MQTT 消息对象
    MQTTClient_freeMessage(&message);
    // 释放主题名字符串
    if (topicName) MQTTClient_free(topicName);
    // 返回 1 表示消息已处理
    return 1;
}

// 尝试重连 MQTT 服务器的函数
static int reconnect_mqtt(mqtt_ctx* ctx) {
    // 初始化连接参数结构体
    MQTTClient_connectOptions conn_opts = MQTTClient_connectOptions_initializer;
    int rc;
    
    // 设置连接参数
    conn_opts.keepAliveInterval = 20; // 保活时间
    conn_opts.cleansession = 1;       // 清除会话
    conn_opts.connectTimeout = DEFAULT_TIMEOUT; // 连接超时时间
    
    // 尝试重新连接
    if ((rc = MQTTClient_connect(ctx->client, &conn_opts)) != MQTTCLIENT_SUCCESS) {
        fprintf(stderr, "重连失败: %d，将在%d毫秒后重试\n", rc, RECONNECT_INTERVAL);
        return rc;
    }
    
    // 重新订阅主题，确保断线重连后还能收到消息
    if ((rc = MQTTClient_subscribe(ctx->client, TOPIC_SUB, DEFAULT_QOS)) != MQTTCLIENT_SUCCESS) {
        fprintf(stderr, "重新订阅失败: %d\n", rc);
        return rc;
    }
    
    printf("MQTT重连成功\n");
    return MQTTCLIENT_SUCCESS;
}

// 连接丢失回调函数
// 当与 MQTT 服务器的连接断开时会被调用
static void connlost(void *context, char *cause) {
    mqtt_ctx* ctx = (mqtt_ctx*)context;
    fprintf(stderr, "连接丢失，原因: %s\n", cause);
    
    // 标记连接状态为断开，后续由主循环处理重连
    ctx->connected = 0;
    // 重连逻辑在 mqtt_loop 中处理
}

// 初始化 MQTT 连接
int mqtt_init(mqtt_ctx* ctx, message_handler handler) {
    MQTTClient_connectOptions conn_opts = MQTTClient_connectOptions_initializer;
    int rc;
    
    // 初始化上下文结构体，清零
    memset(ctx, 0, sizeof(mqtt_ctx));
    ctx->handler = handler;      // 设置用户消息处理回调
    ctx->connected = 0;          // 初始为未连接
    ctx->last_reconnect = 0;     // 上次重连时间初始化
    
    // 创建 MQTT 客户端实例
    if ((rc = MQTTClient_create(&ctx->client, DEFAULT_ADDRESS, DEFAULT_CLIENT_ID,
                              MQTTCLIENT_PERSISTENCE_NONE, NULL)) != MQTTCLIENT_SUCCESS) {
        fprintf(stderr, "创建客户端失败: %d\n", rc);
        return rc;
    }
    
    // 设置回调函数，包括连接丢失、消息到达、消息送达
    if ((rc = MQTTClient_setCallbacks(ctx->client, ctx, connlost, msgarrvd, delivered)) != MQTTCLIENT_SUCCESS) {
        fprintf(stderr, "设置回调失败: %d\n", rc);
        MQTTClient_destroy(&ctx->client);
        return rc;
    }
    
    // 设置连接参数
    conn_opts.keepAliveInterval = 20; // 保活时间
    conn_opts.cleansession = 1;       // 清除会话
    conn_opts.connectTimeout = DEFAULT_TIMEOUT; // 连接超时时间
    
    // 建立与 MQTT 服务器的连接
    if ((rc = MQTTClient_connect(ctx->client, &conn_opts)) != MQTTCLIENT_SUCCESS) {
        fprintf(stderr, "连接失败: %d\n", rc);
        MQTTClient_destroy(&ctx->client);
        return rc;
    }
    
    // 订阅指定主题，确保能收到消息
    if ((rc = MQTTClient_subscribe(ctx->client, TOPIC_SUB, DEFAULT_QOS)) != MQTTCLIENT_SUCCESS) {
        fprintf(stderr, "订阅失败: %d\n", rc);
        MQTTClient_disconnect(ctx->client, DEFAULT_TIMEOUT);
        MQTTClient_destroy(&ctx->client);
        return rc;
    }
    
    ctx->connected = 1; // 标记为已连接
    return MQTTCLIENT_SUCCESS;
}

// 发布消息到指定主题
int mqtt_publish(mqtt_ctx* ctx, const char* topic, 
                const void* payload, size_t payload_len) {
    MQTTClient_message pubmsg = MQTTClient_message_initializer;
    MQTTClient_deliveryToken token;
    int rc;
    
    // 参数检查，确保上下文、主题、载荷有效
    if (!ctx || !topic || !payload || payload_len == 0) {
        fprintf(stderr, "发布参数无效\n");
        return -1;
    }
    
    // 检查连接状态，未连接不能发布
    if (!ctx->connected) {
        fprintf(stderr, "MQTT未连接，无法发布\n");
        return -2;
    }
    
    // 设置消息内容
    pubmsg.payload = (void*)payload;
    pubmsg.payloadlen = (int)payload_len;
    pubmsg.qos = DEFAULT_QOS; // 服务质量
    pubmsg.retained = 0;      // 不保留消息
    
    // 发布消息
    rc = MQTTClient_publishMessage(ctx->client, topic, &pubmsg, &token);
    if (rc != MQTTCLIENT_SUCCESS) {
        fprintf(stderr, "发布失败: %d\n", rc);
        return rc;
    }
    
    // 等待消息送达服务器（可选，保证消息已发送）
    if ((rc = MQTTClient_waitForCompletion(ctx->client, token, DEFAULT_TIMEOUT)) != MQTTCLIENT_SUCCESS) {
        fprintf(stderr, "等待完成失败: %d\n", rc);
    }
    return rc;
}

// MQTT 主循环，需在主线程定期调用
void mqtt_loop(mqtt_ctx* ctx) {
    // 获取当前时间（毫秒）
    struct timespec ts;
    unsigned long current_time;
    
    // 使用 CLOCK_MONOTONIC 获取系统启动到现在的时间，避免系统时间变化影响
    clock_gettime(CLOCK_MONOTONIC, &ts);
    current_time = (unsigned long)(ts.tv_sec * 1000 + ts.tv_nsec / 1000000);
    
    // 检查连接状态
    if (!ctx->connected) {
        // 如果距离上次重连已超过设定间隔，则尝试重连
        if (current_time - ctx->last_reconnect >= RECONNECT_INTERVAL) {
            printf("尝试重新连接MQTT服务器...\n");
            if (reconnect_mqtt(ctx) == MQTTCLIENT_SUCCESS) {
                ctx->connected = 1;
            }
            ctx->last_reconnect = current_time;
        }
    } else {
        // 维持网络流量，处理收发缓冲区（异步模式需要）
        MQTTClient_yield();
    }
}

// 断开 MQTT 连接并释放资源
void mqtt_disconnect(mqtt_ctx* ctx) {
    if (ctx && ctx->client) {
        ctx->connected = 0; // 标记为断开连接
        // 断开与服务器的连接
        MQTTClient_disconnect(ctx->client, DEFAULT_TIMEOUT);
        // 销毁客户端实例，释放资源
        MQTTClient_destroy(&ctx->client);
    }
}