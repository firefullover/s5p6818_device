#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <time.h>
#include <signal.h>
#include <string.h>
#include <config/config.h>
#include <camera/camera_test.h>
#include <engine/engine.h>
#include <mqtt/mqtt.h>

// 全局上下文
static mqtt_ctx g_mqtt_ctx;
volatile static int g_running = 1;
static camera_config_t g_camera_config;
static uint32_t g_frame_id = 0; // 帧ID计数器

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
        
        unsigned char* frame_data = NULL;
        long frame_size = 0;
        
        // 从摄像头获取一帧图像数据
        int ret = camera_get_frame(&frame_data, &frame_size);
        if(ret == 0 && frame_data != NULL && frame_size > 0) {
            consecutive_failures = 0; // 重置失败计数
            
            // 创建带帧头的完整数据包
            frame_header_t header;
            header.frame_id = g_frame_id++;
            header.frame_len = frame_size;
            
            // 分配内存用于存储帧头+帧数据
            size_t total_size = sizeof(frame_header_t) + frame_size;
            unsigned char* mqtt_payload = (unsigned char*)malloc(total_size);
            if (mqtt_payload) {
                // 复制帧头和帧数据到完整数据包
                memcpy(mqtt_payload, &header, sizeof(frame_header_t));
                memcpy(mqtt_payload + sizeof(frame_header_t), frame_data, frame_size);
                
                // 发布到MQTT
                if(mqtt_publish(&g_mqtt_ctx, TOPIC_PUB, mqtt_payload, total_size) != 0) {
                    fprintf(stderr, "图像发布失败\n");
                    consecutive_failures++;
                } else {
                    printf("成功发布图像数据，帧ID: %u, 大小: %ld 字节\n", 
                           header.frame_id, frame_size);
                }
                
                // 释放完整数据包内存
                free(mqtt_payload);
            } else {
                fprintf(stderr, "内存分配失败\n");
                consecutive_failures++;
            }
            
            // 释放帧内存
            free(frame_data);
        } else {
            fprintf(stderr, "获取图像数据失败: %d\n", ret);
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

// 测试 get_image_data 并通过MQTT发送到6818_image主题
void* test_camera_data_publish(void* arg) {
    unsigned char* buffer = NULL;
    long size = 0;
    get_image_data(&buffer, &size);
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

    // 初始化摄像头
    g_camera_config.device = CAMERA_DEVICE;
    g_camera_config.width = 240;
    g_camera_config.height = 240;
    g_camera_config.fps = TARGET_FPS;
    g_camera_config.is_initialized = false;
    
    int camera_ret = camera_init(&g_camera_config);
    if (camera_ret != 0) {
        fprintf(stderr, "摄像头初始化失败，错误码: %d\n", camera_ret);
        return 1;
    }
    printf("摄像头初始化成功\n");

    // 初始化MQTT
    int mqtt_ok = mqtt_init(&g_mqtt_ctx, parse_json_and_control);
    if(mqtt_ok != 0) {
        fprintf(stderr, "MQTT初始化失败\n");
        camera_deinit();
        engine_close();
        return 1;
    }
    printf("MQTT连接成功，已订阅主题: %s\n", TOPIC_SUB);

    // 创建监听线程
    pthread_t listen_tid;
    if(pthread_create(&listen_tid, NULL, mqtt_listen_thread, NULL) != 0) {
        fprintf(stderr, "线程创建失败\n");
        mqtt_disconnect(&g_mqtt_ctx);
        camera_deinit();
        engine_close();
        return 1;
    }

    // 创建视频发布线程
    pthread_t video_tid;
    if(pthread_create(&video_tid, NULL, video_publish_thread, NULL) != 0) {
        fprintf(stderr, "视频发布线程创建失败\n");
        mqtt_disconnect(&g_mqtt_ctx);
        camera_deinit();
        engine_close();
        return 1;
    }
    
    pthread_join(listen_tid, NULL);
    pthread_join(video_tid, NULL);
    
    // 清理资源
    mqtt_disconnect(&g_mqtt_ctx);
    camera_deinit();
    engine_close();
    return 0;
}