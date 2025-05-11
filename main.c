#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <time.h>
#include "config.h"  // 包含配置文件
#include "camera.h"
#include "engine.h"
#include "mqtt.h"

// 全局上下文
static mqtt_ctx g_mqtt_ctx;
static camera_ctx* g_camera_ctx = NULL;
static int g_running = 1;

// 信号处理函数
void sig_handler(int sig) {
    printf("\n收到终止信号，清理资源...\n");
    g_running = 0;
}

// MQTT消息回调函数
void on_mqtt_message(char* topic, char* payload) {
    printf("[MQTT消息] 主题: %s\n内容: %s\n", topic, payload);
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
        
        uint8_t* frame = NULL;
        size_t frame_size = 0;
        
        // 获取摄像头帧
        int cam_result = camera_get_frame(g_camera_ctx, &frame, &frame_size, 500);
        if(cam_result == CAM_SUCCESS) {
            consecutive_failures = 0; // 重置失败计数
            
            // 发布到MQTT
            if(mqtt_publish(&g_mqtt_ctx, TOPIC_PUB, frame, frame_size) != 0) {
                fprintf(stderr, "视频发布失败\n");
                consecutive_failures++;
            }
            
            // 释放帧内存
            camera_release_frame(frame);
        } else {
            fprintf(stderr, "获取摄像头帧失败: %d\n", cam_result);
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

int main() {
    // 注册信号处理
    signal(SIGINT, sig_handler);
    signal(SIGTERM, sig_handler);

    // 初始化舵机
    engine_init();
    printf("舵机初始化完成\n");

    // 初始化摄像头
    int error;
    g_camera_ctx = camera_init("/dev/video0", 640, 480, FMT_JPEG, &error);
    if(!g_camera_ctx) {
        fprintf(stderr, "摄像头初始化失败: %d\n", error);
        return 1;
    }
    printf("摄像头初始化完成\n");

    // 初始化MQTT
    int mqtt_ok = mqtt_init(&g_mqtt_ctx, on_mqtt_message);
    if(mqtt_ok != 0) {
        fprintf(stderr, "MQTT初始化失败\n");
        camera_destroy(g_camera_ctx);
        return 1;
    }
    printf("MQTT连接成功\n");

    // 创建监听线程
    pthread_t listen_tid;
    int thread_ok = pthread_create(&listen_tid, NULL, mqtt_listen_thread, NULL);
    if(thread_ok != 0) {
        fprintf(stderr, "线程创建失败\n");
        mqtt_disconnect(&g_mqtt_ctx);
        camera_destroy(g_camera_ctx);
        engine_close();
        printf("资源释放完成\n");
        return 1;
    }

    // 在主线程运行视频发布
    video_publish_thread(NULL);

    // 等待监听线程结束
    pthread_join(listen_tid, NULL);

    // 清理资源
    printf("正在关闭资源...\n");
    mqtt_disconnect(&g_mqtt_ctx);
    camera_destroy(g_camera_ctx);
    engine_close();
    printf("资源释放完成\n");
    return 0;
}