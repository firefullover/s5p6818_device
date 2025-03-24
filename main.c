#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <pthread.h> 
#include <mosquitto.h>
#include "engine.h"
#include "camera_pub.h"

#define DEFAULT_ADDRESS   "192.168.5.109"  // MQTT 服务器地址（不含协议前缀）
#define DEFAULT_PORT      1883             // MQTT 端口
#define DEFAULT_CLIENT_ID "s5p6818_Client"  // 客户端 ID
#define DEFAULT_QOS       1                // 服务质量等级
#define DEFAULT_TIMEOUT   1000             // 超时时间（毫秒）
#define TOPIC_SUB         "6050_date"      // 订阅主题
#define TOPIC_PUB         "6818_image"     // 发布主题

volatile sig_atomic_t running = 1;         // 控制主循环运行标志

void handle_signal(int s)
{
    printf("\n收到终止信号，清理资源...\n");
    running = 0;
}

// MQTT连接成功自动订阅6050_date主题
void on_connect(struct mosquitto *mosq, void *obj, int rc)
{
    if(rc == 0) {
        printf("已成功连接到 MQTT 服务器。\n");  // 输出连接成功信息
        // 订阅指定主题 TOPIC_SUB，服务质量为 DEFAULT_QOS
        int sub_rc = mosquitto_subscribe(mosq, NULL, TOPIC_SUB, DEFAULT_QOS);
        if(sub_rc != MOSQ_ERR_SUCCESS){
            fprintf(stderr, "订阅失败：%s\n", mosquitto_strerror(sub_rc));  // 输出订阅错误信息
        }
    } else {
        printf("连接失败，错误代码：%d\n", rc);  // 输出连接失败信息
    }
}

// 处理engine角度信息
void *thread_parse_json(void *arg)
{
    char *json_data = (char *)arg;  // 获取传入的数据指针
    if (json_data) {
        parse_json_and_control(json_data);  // 解析 JSON 并控制舵机
        free(json_data);  // 释放分配的内存
    }
    pthread_exit(NULL);  // 线程退出
}

//收到消息自动调用json_parse_thread函数
void on_message(struct mosquitto *mosq, void *obj, const struct mosquitto_message *msg)
{
    if (strcmp(msg->topic, TOPIC_SUB) != 0) {
        printf("收到无关主题 %s 的消息，忽略。\n", msg->topic);
    }

    if (msg->payloadlen) {
        // 分配内存存储消息
        char *json_data = malloc(msg->payloadlen + 1);
        if (json_data) {
            memcpy(json_data, msg->payload, msg->payloadlen);
            json_data[msg->payloadlen] = '\0';  // 确保字符串终止

            // 创建新线程处理 JSON 解析
            pthread_t thread_id;
            if (pthread_create(&thread_id, NULL, thread_parse_json, json_data) != 0) {
                fprintf(stderr, "错误: 无法创建 JSON 解析线程\n");
                free(json_data);  // 创建失败时释放内存
            } else {
                pthread_detach(thread_id);  // 分离线程，防止资源泄露
            }
        } else {
            fprintf(stderr, "分配内存失败\n");
        }
    } else {
        printf("收到信息为空\n");
    }
}

// 发布摄像头数据（测试）
void publish_camera_data() {
    unsigned char *buffer = NULL;
    long size = 0;
    get_camera_data(&buffer, &size);
    if (buffer) {
        int rc = mosquitto_publish(mosq, NULL, TOPIC_PUB, size, buffer, DEFAULT_QOS, false);
        if (rc != MOSQ_ERR_SUCCESS) {
            fprintf(stderr, "发布失败: %s\n", mosquitto_strerror(rc));
        } else {
            printf("已发布摄像头数据 (%ld 字节) 到主题 '%s'\n", size, TOPIC_PUB);
        }
        free(buffer);  // 释放内存
    }
}

int main() {
    struct mosquitto *mosq = NULL;  // 定义 MQTT 客户端指针
    int rc;                         // 定义返回值变量
    signal(SIGINT, handle_signal);
    engine_init();
    reset_engine();// 复位舵机为(90, 90)

    mosquitto_lib_init();// 初始化 mosquitto 库
    mosq = mosquitto_new(DEFAULT_CLIENT_ID, true, NULL);
    if(!mosq) {
        fprintf(stderr, "错误：无法创建 MQTT 客户端。\n");
        mosquitto_lib_cleanup();  // 清理 mosquitto 库资源
        return 1;
    }
    mosquitto_connect_callback_set(mosq, on_connect);
    mosquitto_message_callback_set(mosq, on_message);

    // 连接到 MQTT 服务器
    rc = mosquitto_connect(mosq, DEFAULT_ADDRESS, DEFAULT_PORT, 60);
    if(rc) {
        fprintf(stderr, "无法连接到服务器 (%d)：%s\n", rc, mosquitto_strerror(rc));
        mosquitto_destroy(mosq);   // 销毁 MQTT 客户端实例
        mosquitto_lib_cleanup();   // 清理 mosquitto 库资源
        return 1;
    }
    
    // 启动网络循环处理线程，用于处理网络通信和回调函数
    rc = mosquitto_loop_start(mosq);
    if(rc != MOSQ_ERR_SUCCESS) {
        fprintf(stderr, "无法启动网络循环：%s\n", mosquitto_strerror(rc));
        mosquitto_destroy(mosq);   // 销毁 MQTT 客户端实例
        mosquitto_lib_cleanup();   // 清理 mosquitto 库资源
        return 1;
    }

    // 主循环（示例每秒发布图像）
    while (!finished) {
        sleep(1);
    }

    // 资源清理
    printf("程序退出...\n");

    // 停止网络循环线程，并清理相关资源
    mosquitto_loop_stop(mosq, true);
    mosquitto_disconnect(mosq);
    mosquitto_destroy(mosq);
    mosquitto_lib_cleanup();
    engine_close();

    return 0;
}