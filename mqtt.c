#include "mqtt.h"
#include "engine.h"  // 包含舵机控制头文件

// MQTT消息到达回调
static void delivered(void *context, MQTTClient_deliveryToken dt) {
    // 可添加消息送达确认处理
    printf("消息已送达: %d\n", dt);
}

static int msgarrvd(void *context, char *topicName, int topicLen, 
                   MQTTClient_message *message) {
    mqtt_ctx* ctx = (mqtt_ctx*)context;
    char* payload = NULL;
    
    // 安全检查
    if (!message || !message->payload || message->payloadlen <= 0) {
        fprintf(stderr, "收到无效消息\n");
        goto cleanup;
    }
    
    // 提取有效载荷
    payload = malloc(message->payloadlen + 1);
    if (!payload) {
        fprintf(stderr, "内存分配失败\n");
        goto cleanup;
    }
    
    memcpy(payload, message->payload, message->payloadlen);
    payload[message->payloadlen] = '\0';
    
    // 调用用户定义的消息处理器
    if(ctx->handler) {
        ctx->handler(topicName, payload);
    }
    
    // 示例：直接转发给舵机控制（需根据实际协议调整）
    if(topicName && strcmp(topicName, TOPIC_SUB) == 0) {
        parse_json_and_control(payload); // 调用舵机库的解析函数
    }
    
cleanup:
    if (payload) free(payload);
    MQTTClient_freeMessage(&message);
    if (topicName) MQTTClient_free(topicName);
    return 1;
}

// 重连函数
static int reconnect_mqtt(mqtt_ctx* ctx) {
    MQTTClient_connectOptions conn_opts = MQTTClient_connectOptions_initializer;
    int rc;
    
    // 连接参数
    conn_opts.keepAliveInterval = 20;
    conn_opts.cleansession = 1;
    conn_opts.connectTimeout = DEFAULT_TIMEOUT;
    
    // 尝试重新连接
    if ((rc = MQTTClient_connect(ctx->client, &conn_opts)) != MQTTCLIENT_SUCCESS) {
        fprintf(stderr, "重连失败: %d，将在%d毫秒后重试\n", rc, RECONNECT_INTERVAL);
        return rc;
    }
    
    // 重新订阅主题
    if ((rc = MQTTClient_subscribe(ctx->client, TOPIC_SUB, DEFAULT_QOS)) != MQTTCLIENT_SUCCESS) {
        fprintf(stderr, "重新订阅失败: %d\n", rc);
        return rc;
    }
    
    printf("MQTT重连成功\n");
    return MQTTCLIENT_SUCCESS;
}

static void connlost(void *context, char *cause) {
    mqtt_ctx* ctx = (mqtt_ctx*)context;
    fprintf(stderr, "连接丢失，原因: %s\n", cause);
    
    // 标记连接状态为断开
    ctx->connected = 0;
    // 重连逻辑在mqtt_loop中处理
}

int mqtt_init(mqtt_ctx* ctx, message_handler handler) {
    MQTTClient_connectOptions conn_opts = MQTTClient_connectOptions_initializer;
    int rc;
    
    // 初始化上下文
    memset(ctx, 0, sizeof(mqtt_ctx));
    ctx->handler = handler;
    ctx->connected = 0;
    ctx->last_reconnect = 0;
    
    // 创建客户端实例
    if ((rc = MQTTClient_create(&ctx->client, DEFAULT_ADDRESS, DEFAULT_CLIENT_ID,
                              MQTTCLIENT_PERSISTENCE_NONE, NULL)) != MQTTCLIENT_SUCCESS) {
        fprintf(stderr, "创建客户端失败: %d\n", rc);
        return rc;
    }
    
    // 设置回调
    if ((rc = MQTTClient_setCallbacks(ctx->client, ctx, connlost, msgarrvd, delivered)) != MQTTCLIENT_SUCCESS) {
        fprintf(stderr, "设置回调失败: %d\n", rc);
        MQTTClient_destroy(&ctx->client);
        return rc;
    }
    
    // 连接参数
    conn_opts.keepAliveInterval = 20;
    conn_opts.cleansession = 1;
    conn_opts.connectTimeout = DEFAULT_TIMEOUT;
    
    // 建立连接
    if ((rc = MQTTClient_connect(ctx->client, &conn_opts)) != MQTTCLIENT_SUCCESS) {
        fprintf(stderr, "连接失败: %d\n", rc);
        MQTTClient_destroy(&ctx->client);
        return rc;
    }
    
    // 订阅主题
    if ((rc = MQTTClient_subscribe(ctx->client, TOPIC_SUB, DEFAULT_QOS)) != MQTTCLIENT_SUCCESS) {
        fprintf(stderr, "订阅失败: %d\n", rc);
        MQTTClient_disconnect(ctx->client, DEFAULT_TIMEOUT);
        MQTTClient_destroy(&ctx->client);
        return rc;
    }
    
    ctx->connected = 1;
    return MQTTCLIENT_SUCCESS;
}

int mqtt_publish(mqtt_ctx* ctx, const char* topic, 
                const void* payload, size_t payload_len) {
    MQTTClient_message pubmsg = MQTTClient_message_initializer;
    MQTTClient_deliveryToken token;
    int rc;
    
    // 参数检查
    if (!ctx || !topic || !payload || payload_len == 0) {
        fprintf(stderr, "发布参数无效\n");
        return -1;
    }
    
    // 检查连接状态
    if (!ctx->connected) {
        fprintf(stderr, "MQTT未连接，无法发布\n");
        return -2;
    }
    
    pubmsg.payload = (void*)payload;
    pubmsg.payloadlen = (int)payload_len;
    pubmsg.qos = DEFAULT_QOS;
    pubmsg.retained = 0;
    
    rc = MQTTClient_publishMessage(ctx->client, topic, &pubmsg, &token);
    if (rc != MQTTCLIENT_SUCCESS) {
        fprintf(stderr, "发布失败: %d\n", rc);
        return rc;
    }
    
    // 等待消息送达（可选）
    if ((rc = MQTTClient_waitForCompletion(ctx->client, token, DEFAULT_TIMEOUT)) != MQTTCLIENT_SUCCESS) {
        fprintf(stderr, "等待完成失败: %d\n", rc);
    }
    return rc;
}

void mqtt_loop(mqtt_ctx* ctx) {
    // 获取当前时间（毫秒）
    struct timespec ts;
    unsigned long current_time;
    
    // 使用CLOCK_MONOTONIC获取更精确的时间
    clock_gettime(CLOCK_MONOTONIC, &ts);
    current_time = (unsigned long)(ts.tv_sec * 1000 + ts.tv_nsec / 1000000);
    
    // 检查连接状态
    if (!ctx->connected) {
        // 检查是否到达重连间隔
        if (current_time - ctx->last_reconnect >= RECONNECT_INTERVAL) {
            printf("尝试重新连接MQTT服务器...\n");
            if (reconnect_mqtt(ctx) == MQTTCLIENT_SUCCESS) {
                ctx->connected = 1;
            }
            ctx->last_reconnect = current_time;
        }
    } else {
        // 维持网络流量（异步模式需要）
        MQTTClient_yield();
    }
}

void mqtt_disconnect(mqtt_ctx* ctx) {
    if (ctx && ctx->client) {
        ctx->connected = 0; // 标记为断开连接
        MQTTClient_disconnect(ctx->client, DEFAULT_TIMEOUT);
        MQTTClient_destroy(&ctx->client);
    }
}