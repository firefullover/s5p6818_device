#include <stdio.h>
#include <stdlib.h>
#include <jpeglib.h>
#include <stdint.h>

// 最近邻缩放
void resize_nearest(uint8_t* src, int sw, int sh, uint8_t* dst, int dw, int dh) {
    for (int y = 0; y < dh; ++y) {
        int sy = y * sh / dh;
        for (int x = 0; x < dw; ++x) {
            int sx = x * sw / dw;
            for (int c = 0; c < 3; ++c) {
                dst[(y*dw + x)*3 + c] = src[(sy*sw + sx)*3 + c];
            }
        }
    }
}

// RGB888转RGB565
uint16_t rgb888_to_rgb565(uint8_t r, uint8_t g, uint8_t b) {
    return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
}

int main() {
    const char* jpg_path = "firefly.jpg";
    const char* rgb_path = "../build/image.rgb";
    int target_w = 240, target_h = 240;

    // 1. 解码JPEG
    FILE* infile = fopen(jpg_path, "rb");
    if (!infile) {
        perror("无法打开jpg文件");
        return 1;
    }
    struct jpeg_decompress_struct cinfo;
    struct jpeg_error_mgr jerr;
    cinfo.err = jpeg_std_error(&jerr);
    jpeg_create_decompress(&cinfo);
    jpeg_stdio_src(&cinfo, infile);
    jpeg_read_header(&cinfo, TRUE);
    jpeg_start_decompress(&cinfo);

    int sw = cinfo.output_width;
    int sh = cinfo.output_height;
    int sc = cinfo.output_components; // 通常为3
    if (sc != 3) {
        fprintf(stderr, "只支持RGB彩色jpg\n");
        fclose(infile);
        jpeg_destroy_decompress(&cinfo);
        return 1;
    }
    uint8_t* srcbuf = malloc(sw * sh * 3);
    uint8_t* p = srcbuf;
    while (cinfo.output_scanline < sh) {
        JSAMPROW row_pointer[1];
        row_pointer[0] = p + cinfo.output_scanline * sw * 3;
        jpeg_read_scanlines(&cinfo, row_pointer, 1);
    }
    jpeg_finish_decompress(&cinfo);
    jpeg_destroy_decompress(&cinfo);
    fclose(infile);

    // 2. 缩放到240x240
    uint8_t* dstbuf = malloc(target_w * target_h * 3);
    resize_nearest(srcbuf, sw, sh, dstbuf, target_w, target_h);
    free(srcbuf);

    // 3. 转为RGB565并写入文件
    FILE* outfile = fopen(rgb_path, "wb");
    if (!outfile) {
        perror("无法创建rgb文件");
        free(dstbuf);
        return 1;
    }
    for (int i = 0; i < target_w * target_h; ++i) {
        uint8_t r = dstbuf[i*3+0];
        uint8_t g = dstbuf[i*3+1];
        uint8_t b = dstbuf[i*3+2];
        uint16_t rgb565 = rgb888_to_rgb565(r, g, b);
        fwrite(&rgb565, 2, 1, outfile);
    }
    fclose(outfile);
    free(dstbuf);
    printf("转换完成，输出文件: %s\n", rgb_path);
    return 0;
}
