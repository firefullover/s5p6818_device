#ifndef CONFIG_H
#define CONFIG_H

// ===================== MQTT配置 =====================
// MQTT服务器地址
#define DEFAULT_ADDRESS   "tcp://192.168.1.95:1883" 
// MQTT客户端ID，需唯一
#define DEFAULT_CLIENT_ID "s5p6818_Client" // 设备标识名
// MQTT消息服务质量等级，0=最多一次，1=至少一次，2=仅一次
#define DEFAULT_QOS       1 // 推荐1，保证消息至少送达一次
// MQTT操作超时时间（毫秒）
#define DEFAULT_TIMEOUT   1000 // 连接、发布等操作的超时时间
// 订阅主题名称，接收控制指令
#define TOPIC_SUB         "6050_date" // 订阅的主题，通常为下行指令
// 发布主题名称，上传数据
#define TOPIC_PUB         "6818_image" // 发布的主题，通常为上行数据

// ===================== 重连配置 =====================
// MQTT断线重连间隔（毫秒）
#define RECONNECT_INTERVAL 5000  // 每隔5秒尝试重连一次
// 最大重连尝试次数，超过后放弃重连
#define MAX_RECONNECT_ATTEMPTS 10 // 0为无限重试，建议设置上限防止死循环

// ===================== 视频配置 =====================
// 摄像头设备文件路径
#define CAMERA_DEVICE     "/dev/video0"
// 目标视频帧率（FPS），影响视频流畅度与带宽
#define TARGET_FPS        10     // 建议5~30，过高占用带宽
// 最大连续获取帧失败次数，超过后暂停一段时间
#define MAX_FAILURES      5      // 防止摄像头异常导致死循环

// ===================== 舵机配置 =====================
// 舵机设备文件路径
#define ENGINE_DEVICE     "/dev/myengine" // 需与驱动一致
// 舵机每步对应的角度单位（度）
#define DEG_UNIT          1.8    // 常见步进电机为1.8度/步
// 舵机控制指令间的延迟（毫秒）
#define DELAY_MS          50     // 适当延迟防止过载

#endif