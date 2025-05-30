#include <camera/camera_test.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/time.h>

// FFmpeg库头文件
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavdevice/avdevice.h>
#include <libswscale/swscale.h>
#include <libavutil/imgutils.h>

#define IMAGE_FILE "image.rgb"  // 临时测试文件，存储图像数据的文件路径
#define RGB565_PIXEL_SIZE 2     // 16位RGB565格式每像素占2字节
#define TARGET_WIDTH 240        // 目标图像宽度
#define TARGET_HEIGHT 240       // 目标图像高度
#define TARGET_SIZE (TARGET_WIDTH * TARGET_HEIGHT * RGB565_PIXEL_SIZE) // 目标图像大小

// FFmpeg相关全局变量
static AVFormatContext *format_ctx = NULL;
static AVCodecContext *codec_ctx = NULL;
static AVFrame *frame = NULL;
static AVFrame *rgb_frame = NULL;
static struct SwsContext *sws_ctx = NULL;
static AVPacket packet;
static uint8_t *rgb_buffer = NULL;
static int video_stream_index = -1;
static uint32_t frame_counter = 0;
static bool camera_ready = false;

// 初始化摄像头，设置参数并打开设备
int camera_init(camera_config_t *config) {
    int ret;
    AVCodec *codec = NULL;
    AVDictionary *options = NULL;
    char device_format[64];
    
    // 参数检查
    if (!config || !config->device) {
        fprintf(stderr, "摄像头配置无效\n");
        return -1;
    }
    
    // 如果已经初始化，先释放资源
    if (camera_ready) {
        camera_deinit();
    }
    
    // 注册所有设备和编解码器
    avdevice_register_all();
    
    // 创建输入格式上下文
    format_ctx = avformat_alloc_context();
    if (!format_ctx) {
        fprintf(stderr, "无法分配AVFormatContext\n");
        return -1;
    }
    
    // 设置设备选项
    snprintf(device_format, sizeof(device_format), "video=%s", config->device);
    av_dict_set(&options, "framerate", "30", 0); // 设置采集帧率
    av_dict_set(&options, "video_size", "320x240", 0); // 设置采集分辨率为320x240（最接近目标240x240的支持分辨率）
    av_dict_set(&options, "input_format", "mjpeg", 0); // 使用MJPEG格式（摄像头支持）
    
    // 打开视频设备
    AVInputFormat *input_format = av_find_input_format("v4l2"); // Linux下使用V4L2
    if (!input_format) {
        fprintf(stderr, "找不到v4l2输入格式\n");
        avformat_free_context(format_ctx);
        return -1;
    }
    
    ret = avformat_open_input(&format_ctx, config->device, input_format, &options);
    if (ret < 0) {
        char err_buf[128];
        av_strerror(ret, err_buf, sizeof(err_buf));
        fprintf(stderr, "无法打开摄像头设备: %s\n", err_buf);
        avformat_free_context(format_ctx);
        return -1;
    }
    
    // 获取流信息
    ret = avformat_find_stream_info(format_ctx, NULL);
    if (ret < 0) {
        fprintf(stderr, "无法获取流信息\n");
        avformat_close_input(&format_ctx);
        return -1;
    }
    
    // 查找视频流
    video_stream_index = -1;
    for (int i = 0; i < format_ctx->nb_streams; i++) {
        if (format_ctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
            video_stream_index = i;
            break;
        }
    }
    
    if (video_stream_index == -1) {
        fprintf(stderr, "找不到视频流\n");
        avformat_close_input(&format_ctx);
        return -1;
    }
    
    // 获取解码器
    codec = avcodec_find_decoder(format_ctx->streams[video_stream_index]->codecpar->codec_id);
    if (!codec) {
        fprintf(stderr, "找不到解码器\n");
        avformat_close_input(&format_ctx);
        return -1;
    }
    
    // 创建解码器上下文
    codec_ctx = avcodec_alloc_context3(codec);
    if (!codec_ctx) {
        fprintf(stderr, "无法分配解码器上下文\n");
        avformat_close_input(&format_ctx);
        return -1;
    }
    
    // 从流中复制编解码器参数到解码器上下文
    ret = avcodec_parameters_to_context(codec_ctx, format_ctx->streams[video_stream_index]->codecpar);
    if (ret < 0) {
        fprintf(stderr, "无法复制编解码器参数\n");
        avcodec_free_context(&codec_ctx);
        avformat_close_input(&format_ctx);
        return -1;
    }
    
    // 打开解码器
    ret = avcodec_open2(codec_ctx, codec, NULL);
    if (ret < 0) {
        fprintf(stderr, "无法打开解码器\n");
        avcodec_free_context(&codec_ctx);
        avformat_close_input(&format_ctx);
        return -1;
    }
    
    // 分配帧缓冲区
    frame = av_frame_alloc();
    rgb_frame = av_frame_alloc();
    if (!frame || !rgb_frame) {
        fprintf(stderr, "无法分配帧缓冲区\n");
        if (frame) av_frame_free(&frame);
        if (rgb_frame) av_frame_free(&rgb_frame);
        avcodec_free_context(&codec_ctx);
        avformat_close_input(&format_ctx);
        return -1;
    }
    
    // 分配RGB缓冲区
    rgb_buffer = (uint8_t *)av_malloc(av_image_get_buffer_size(AV_PIX_FMT_RGB565, TARGET_WIDTH, TARGET_HEIGHT, 1));
    if (!rgb_buffer) {
        fprintf(stderr, "无法分配RGB缓冲区\n");
        av_frame_free(&frame);
        av_frame_free(&rgb_frame);
        avcodec_free_context(&codec_ctx);
        avformat_close_input(&format_ctx);
        return -1;
    }
    
    // 设置RGB帧的参数
    av_image_fill_arrays(rgb_frame->data, rgb_frame->linesize, rgb_buffer,
                         AV_PIX_FMT_RGB565, TARGET_WIDTH, TARGET_HEIGHT, 1);
    
    // 初始化图像转换上下文
    sws_ctx = sws_getContext(codec_ctx->width, codec_ctx->height, codec_ctx->pix_fmt,
                            TARGET_WIDTH, TARGET_HEIGHT, AV_PIX_FMT_RGB565,
                            SWS_BILINEAR, NULL, NULL, NULL);
    if (!sws_ctx) {
        fprintf(stderr, "无法创建图像转换上下文\n");
        av_free(rgb_buffer);
        av_frame_free(&frame);
        av_frame_free(&rgb_frame);
        avcodec_free_context(&codec_ctx);
        avformat_close_input(&format_ctx);
        return -1;
    }
    
    // 初始化帧计数器
    frame_counter = 0;
    
    // 标记摄像头已准备好
    camera_ready = true;
    config->is_initialized = true;
    
    printf("摄像头初始化成功: %s, 分辨率: %dx%d\n", 
           config->device, TARGET_WIDTH, TARGET_HEIGHT);
    
    return 0;
}

// 从摄像头获取一帧图像，转换为240*240*16位RGB格式
int camera_get_frame(unsigned char **buffer, long *size) {
    int ret;
    int got_frame = 0;
    struct timeval start, end;
    long elapsed;
    
    // 检查摄像头是否已初始化
    if (!camera_ready) {
        fprintf(stderr, "摄像头未初始化\n");
        return -1;
    }
    
    // 记录开始时间
    gettimeofday(&start, NULL);
    
    // 读取帧直到获取到一个完整的帧
    while (!got_frame) {
        // 读取一个数据包
        ret = av_read_frame(format_ctx, &packet);
        if (ret < 0) {
            fprintf(stderr, "读取帧失败\n");
            return -1;
        }
        
        // 检查是否是视频流的数据包
        if (packet.stream_index == video_stream_index) {
            // 发送数据包到解码器
            ret = avcodec_send_packet(codec_ctx, &packet);
            if (ret < 0) {
                av_packet_unref(&packet);
                fprintf(stderr, "发送数据包到解码器失败\n");
                return -1;
            }
            
            // 从解码器接收帧
            ret = avcodec_receive_frame(codec_ctx, frame);
            if (ret == 0) {
                got_frame = 1;
            } else if (ret == AVERROR(EAGAIN)) {
                // 需要更多数据包
                av_packet_unref(&packet);
                continue;
            } else {
                av_packet_unref(&packet);
                fprintf(stderr, "从解码器接收帧失败\n");
                return -1;
            }
        }
        
        // 释放数据包
        av_packet_unref(&packet);
        
        // 如果获取到帧，跳出循环
        if (got_frame) {
            break;
        }
    }
    
    // 转换图像格式为RGB565
    sws_scale(sws_ctx, (const uint8_t * const*)frame->data, frame->linesize, 0,
              codec_ctx->height, rgb_frame->data, rgb_frame->linesize);
    
    // 分配输出缓冲区
    *size = TARGET_SIZE;
    *buffer = (unsigned char *)malloc(*size);
    if (!*buffer) {
        fprintf(stderr, "无法分配输出缓冲区\n");
        return -1;
    }
    
    // 复制RGB565数据到输出缓冲区
    memcpy(*buffer, rgb_frame->data[0], *size);
    
    // 增加帧计数器
    frame_counter++;
    
    // 计算处理时间
    gettimeofday(&end, NULL);
    elapsed = (end.tv_sec - start.tv_sec) * 1000 + (end.tv_usec - start.tv_usec) / 1000;
    
    // 打印帧率信息（每30帧）
    if (frame_counter % 30 == 0) {
        printf("摄像头帧率: %.2f fps (处理时间: %ld ms)\n", 1000.0 / elapsed, elapsed);
    }
    
    return 0;
}

// 关闭摄像头，释放资源
void camera_deinit(void) {
    if (!camera_ready) {
        return;
    }
    
    // 释放资源
    if (sws_ctx) {
        sws_freeContext(sws_ctx);
        sws_ctx = NULL;
    }
    
    if (rgb_buffer) {
        av_free(rgb_buffer);
        rgb_buffer = NULL;
    }
    
    if (frame) {
        av_frame_free(&frame);
        frame = NULL;
    }
    
    if (rgb_frame) {
        av_frame_free(&rgb_frame);
        rgb_frame = NULL;
    }
    
    if (codec_ctx) {
        avcodec_free_context(&codec_ctx);
        codec_ctx = NULL;
    }
    
    if (format_ctx) {
        avformat_close_input(&format_ctx);
        format_ctx = NULL;
    }
    
    camera_ready = false;
    printf("摄像头已关闭\n");
}

// 兼容旧接口（从文件读取图像数据）
void get_image_data(unsigned char **buffer, long *size) {
    // 打开图像文件，以二进制方式读取
    FILE *file = fopen(IMAGE_FILE, "rb");
    if (!file) {
        perror("无法打开图像文件");
        *buffer = NULL;
        return;
    }

    // 将文件指针移动到文件末尾，获取文件的大小
    fseek(file, 0, SEEK_END);
    *size = ftell(file);
    rewind(file);

    // 校验文件大小是否为 240*240*2 字节
    const long required_size = 240 * 240 * 2;
    if (*size != required_size) {
        fprintf(stderr, "图像文件大小不正确，必须为 %ld 字节，实际为 %ld 字节\n", required_size, *size);
        fclose(file);
        *buffer = NULL;
        *size = 0;
        return;
    }

    // 为 buffer 分配足够的内存来存储图像数据
    *buffer = (unsigned char *)malloc(*size);
    if (!*buffer) {
        perror("内存分配失败");
        fclose(file);
        return;
    }

    // 从文件中读取图像数据到 buffer 中
    if (fread(*buffer, 1, *size, file) != *size) {
        fprintf(stderr, "图像读取不完整\n");
        free(*buffer);
        *buffer = NULL;
    }

    fclose(file);
}

