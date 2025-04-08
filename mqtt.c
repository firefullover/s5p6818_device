/* mqtt.c */
#include "mqtt.h"
#include "engine.h"  // 包含舵机控制头文件

// MQTT消息到达回调
static void delivered(void *context, MQTTClient_deliveryToken dt) {
    // 可添加消息送达确认处理
}

static int msgarrvd(void *context, char *topicName, int topicLen, 
                   MQTTClient_message *message) {
    mqtt_ctx* ctx = (mqtt_ctx*)context;
    
    // 提取有效载荷
    char* payload = malloc(message->payloadlen + 1);
    memcpy(payload, message->payload, message->payloadlen);
    payload[message->payloadlen] = '\0';
    
    // 调用用户定义的消息处理器
    if(ctx->handler) {
        ctx->handler(topicName, payload);
    }
    
    // 示例：直接转发给舵机控制（需根据实际协议调整）
    if(strcmp(topicName, TOPIC_SUB) == 0) {
        parse_json_and_control(payload); // 调用舵机库的解析函数
    }
    
    free(payload);
    MQTTClient_freeMessage(&message);
    MQTTClient_free(topicName);
    return 1;
}

static void connlost(void *context, char *cause) {
    fprintf(stderr, "连接丢失，原因: %s\n", cause);
    // 可添加重连逻辑
}

int mqtt_init(mqtt_ctx* ctx, message_handler handler) {
    MQTTClient_connectOptions conn_opts = MQTTClient_connectOptions_initializer;
    int rc;
    
    // 创建客户端实例
    if ((rc = MQTTClient_create(&ctx->client, DEFAULT_ADDRESS, DEFAULT_CLIENT_ID,
                              MQTTCLIENT_PERSISTENCE_NONE, NULL)) != MQTTCLIENT_SUCCESS) {
        fprintf(stderr, "创建客户端失败: %d\n", rc);
        return rc;
    }
    
    // 设置回调
    if ((rc = MQTTClient_setCallbacks(ctx->client, ctx, connlost, msgarrvd, delivered)) != MQTTCLIENT_SUCCESS) {
        fprintf(stderr, "设置回调失败: %d\n", rc);
        return rc;
    }
    
    // 连接参数
    conn_opts.keepAliveInterval = 20;
    conn_opts.cleansession = 1;
    conn_opts.connectTimeout = DEFAULT_TIMEOUT;
    
    // 建立连接
    if ((rc = MQTTClient_connect(ctx->client, &conn_opts)) != MQTTCLIENT_SUCCESS) {
        fprintf(stderr, "连接失败: %d\n", rc);
        return rc;
    }
    
    // 订阅主题
    if ((rc = MQTTClient_subscribe(ctx->client, TOPIC_SUB, DEFAULT_QOS)) != MQTTCLIENT_SUCCESS) {
        fprintf(stderr, "订阅失败: %d\n", rc);
        return rc;
    }
    
    ctx->handler = handler;
    return MQTTCLIENT_SUCCESS;
}

int mqtt_publish(mqtt_ctx* ctx, const char* topic, 
                const void* payload, size_t payload_len) {
    MQTTClient_message pubmsg = MQTTClient_message_initializer;
    MQTTClient_deliveryToken token;
    
    pubmsg.payload = (void*)payload;
    pubmsg.payloadlen = (int)payload_len;
    pubmsg.qos = DEFAULT_QOS;
    pubmsg.retained = 0;
    
    int rc = MQTTClient_publishMessage(ctx->client, topic, &pubmsg, &token);
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
    // 维持网络流量（异步模式需要）
    MQTTClient_yield();
}

void mqtt_disconnect(mqtt_ctx* ctx) {
    MQTTClient_disconnect(ctx->client, DEFAULT_TIMEOUT);
    MQTTClient_destroy(&ctx->client);
}