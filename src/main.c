#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <time.h>
#include <signal.h>
#include "config/config.h"
#include "camera/camera_pub.h"
#include "engine/engine.h"
#include "mqtt/mqtt.h"

// 全局上下文
static mqtt_ctx g_mqtt_ctx;
volatile static int g_running = 1;

// 信号处理函数
void sig_handler(int sig) {
    printf("\n收到终止信号，清理资源...\n");
    g_running = 0;
}

// 视频发布线程函数（主线程）
void* video_publish_thread(void* arg) {
    (void)arg;
    
    // 帧率控制变量
    struct timespec start_time, end_time;
    long elapsed_us, sleep_us;
    const long target_frame_time_us = 1000000 / TARGET_FPS; // 根据目标帧率计算帧间隔
    int consecutive_failures = 0;
    const int max_failures = MAX_FAILURES; // 最大连续失败次数
    
    while(g_running) {
        clock_gettime(CLOCK_MONOTONIC, &start_time);
        
        unsigned char* frame = NULL;
        long frame_size = 0;
        
        // 获取图像数据（从文件读取）
        get_camera_data(&frame, &frame_size);
        if(frame != NULL && frame_size > 0) {
            consecutive_failures = 0; // 重置失败计数
            
            // 发布到MQTT
            if(mqtt_publish(&g_mqtt_ctx, TOPIC_PUB, frame, frame_size) != 0) {
                fprintf(stderr, "图像发布失败\n");
                consecutive_failures++;
            } else {
                printf("成功发布图像数据，大小: %ld 字节\n", frame_size);
            }
            
            // 释放帧内存
            free(frame);
        } else {
            fprintf(stderr, "获取图像数据失败\n");
            consecutive_failures++;
            
            // 如果连续失败次数过多，暂停一段时间
            if (consecutive_failures >= max_failures) {
                fprintf(stderr, "连续失败次数过多，暂停1秒\n");
                sleep(1);
                consecutive_failures = 0;
            }
        }
        
        // 精确帧率控制
        clock_gettime(CLOCK_MONOTONIC, &end_time);
        elapsed_us = (end_time.tv_sec - start_time.tv_sec) * 1000000 + 
                     (end_time.tv_nsec - start_time.tv_nsec) / 1000;
        
        sleep_us = target_frame_time_us - elapsed_us;
        if (sleep_us > 0) {
            usleep(sleep_us);
        }
    }
    return NULL;
}

// MQTT监听线程函数
void* mqtt_listen_thread(void* arg) {
    (void)arg;
    
    while(g_running) {
        // 维持MQTT连接（非阻塞）
        mqtt_loop(&g_mqtt_ctx);
        usleep(10000); // 10ms检查间隔
    }
    return NULL;
}

// 测试 get_camera_data 并通过MQTT发送到6818_image主题
void test_camera_data_publish() {
    unsigned char* buffer = NULL;
    long size = 0;
    get_camera_data(&buffer, &size);
    if (buffer && size > 0) {
        printf("[TEST] 成功读取图像数据，大小: %ld 字节，准备发送到6818_image主题\n", size);
        if (mqtt_publish(&g_mqtt_ctx, "6818_image", buffer, size) == 0) {
            printf("[TEST] 成功通过MQTT发送到6818_image主题\n");
        } else {
            printf("[TEST] 通过MQTT发送失败\n");
        }
        free(buffer);
    } else {
        printf("[TEST] 读取图像数据失败\n");
    }
}

int main() {
    // 注册信号处理
    signal(SIGINT, sig_handler);
    signal(SIGTERM, sig_handler);

    // // 初始化舵机
    // if (engine_init() != 0) {
    //     printf("舵机初始化失败，程序退出。\n");
    //     return -1;
    // }

    // 初始化MQTT
    int mqtt_ok = mqtt_init(&g_mqtt_ctx, NULL);
    if(mqtt_ok != 0) {
        fprintf(stderr, "MQTT初始化失败\n");
        engine_close();
        return 1;
    }
    printf("MQTT连接成功，已订阅主题: %s\n", TOPIC_SUB);

    

    // 创建监听线程
    pthread_t listen_tid;
    if(pthread_create(&listen_tid, NULL, mqtt_listen_thread, NULL) != 0) {
        fprintf(stderr, "线程创建失败\n");
        mqtt_disconnect(&g_mqtt_ctx);
        engine_close();
        return 1;
    }

    // 在主线程运行视频发布
    // video_publish_thread(NULL);
    // 测试：读取图像并发送到6818_image主题
    test_camera_data_publish();

    // 等待监听线程结束
    printf("MQTT持续监听...\n");
    pthread_join(listen_tid, NULL);
    
    printf("正在关闭资源...\n");
    sleep(5);

    // 清理资源
    mqtt_disconnect(&g_mqtt_ctx);
    // engine_close();
    return 0;
}