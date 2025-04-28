#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <math.h>
#include <limits.h>
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

// 图像处理参数定义
#define MAX_IMAGE_HEIGHT 240
#define MAX_IMAGE_WIDTH 320
#define GrayScale 256
#define grayscale 256

// 边缘提取参数
#define jidian_search_line 120
#define search_start_line 120
#define search_end_line 30
#define left_line_right_scarch 10
#define left_line_left_scarch 5
#define right_line_left_scarch 10
#define right_line_right_scarch 5
#define MID_W 93  // 中点值
#define mid_line_limit 30

// 全局变量
uint8_t base_image[MAX_IMAGE_HEIGHT][MAX_IMAGE_WIDTH];  // 原始灰度图
uint8_t image[MAX_IMAGE_HEIGHT][MAX_IMAGE_WIDTH];       // 二值化图
uint8_t display_image[MAX_IMAGE_HEIGHT][MAX_IMAGE_WIDTH][3]; // 用于显示的RGB图像
uint8_t img_threshold;
uint8_t left_jidian;   // 左边基准点
uint8_t right_jidian;  // 右边基准点
uint8_t left_line_list[MAX_IMAGE_HEIGHT];   // 左边线
uint8_t right_line_list[MAX_IMAGE_HEIGHT];  // 右边线
uint8_t mid_line_list[MAX_IMAGE_HEIGHT];    // 中线
uint8_t final_mid_line;                     // 最终中线位置

// 图像尺寸
int MT9V03X_H = 0;
int MT9V03X_W = 0;

// 直方图相关变量
uint16_t hist[GrayScale] = {0};  // 灰度像素计数器
float P[GrayScale] = {0};        // 每个灰度值的概率
float PK[GrayScale] = {0};       // 概率累计和
float MK[GrayScale] = {0};       // 灰度值累计
float imgsize;                   // 图像大小

// 保存处理阶段图像的函数 - 修复版本
void save_stage_image(const char* base_path, const char* stage_name,
                      uint8_t img[MAX_IMAGE_HEIGHT][MAX_IMAGE_WIDTH], int is_binary) {
    char filename[256];
    sprintf(filename, "%s_%s.png", base_path, stage_name);

    // 创建临时一维数组
    uint8_t* temp_img;
    int channels = is_binary ? 3 : 1;
    int stride = MT9V03X_W * channels;

    temp_img = (uint8_t*)malloc(MT9V03X_H * stride);
    if (!temp_img) {
        printf("内存分配失败\n");
        return;
    }

    for (int i = 0; i < MT9V03X_H; i++) {
        for (int j = 0; j < MT9V03X_W; j++) {
            if (is_binary) {
                // 二值图像转RGB
                temp_img[i * stride + j * 3] = img[i][j];
                temp_img[i * stride + j * 3 + 1] = img[i][j];
                temp_img[i * stride + j * 3 + 2] = img[i][j];
            } else {
                // 灰度图像直接复制
                temp_img[i * stride + j] = img[i][j];
            }
        }
    }

    stbi_write_png(filename, MT9V03X_W, MT9V03X_H, channels, temp_img, stride);
    free(temp_img);

    printf("保存%s阶段图像到: %s\n", stage_name, filename);
}

// 限制函数
uint8_t Limit_uint8(uint8_t min, uint8_t value, uint8_t max) {
    if(value < min) return min;
    if(value > max) return max;
    return value;
}

int Limit_int(int min, int value, int max) {
    if(value < min) return min;
    if(value > max) return max;
    return value;
}

// Otsu阈值计算
uint8_t Ostu(uint8_t index[MAX_IMAGE_HEIGHT][MAX_IMAGE_WIDTH]) {
    uint8_t threshold;
    imgsize = MT9V03X_H * MT9V03X_W;
    uint8_t images_value_temp;

    float sumPK = 0;
    float sumMK = 0;
    float var = 0;
    float vartmp = 0;

    // 清空全部数组
    for(uint16_t i = 0; i < GrayScale; i++) {
        hist[i] = 0;
        P[i] = 0;
        PK[i] = 0;
        MK[i] = 0;
    }

    // 获取直方图
    for(uint8_t i = 0; i < MT9V03X_H; i++) {
        for(uint8_t j = 0; j < MT9V03X_W; j++) {
            images_value_temp = index[i][j];
            hist[images_value_temp]++;
        }
    }

    // 计算方差
    for(uint16_t i = 0; i < GrayScale; i++) {
        P[i] = (float)hist[i]/imgsize; // 计算概率
        PK[i] = sumPK + P[i];
        sumPK = PK[i];
        MK[i] = sumMK + i * P[i];
        sumMK = MK[i];
    }

    // 计算类间最大方差下的阈值
    for(uint8_t i = 5; i < 245; i++) {
        vartmp = ((MK[GrayScale-1]*PK[i]-MK[i])*(MK[GrayScale-1]*PK[i]-MK[i]))/(PK[i]*(1-PK[i]));
        if(vartmp > var) {
            var = vartmp;
            threshold = i;
        }
    }
    return threshold; // 返回阈值
}

// 图像二值化处理
void set_image_twovalues(uint8_t value) {
    uint8_t temp_value;
    for(uint8_t i = 0; i < MT9V03X_H; i++) {
        for(uint8_t j = 0; j < MT9V03X_W; j++) {
            temp_value = base_image[i][j];
            if(temp_value < value) {
                image[i][j] = 0;
            } else {
                image[i][j] = 255;
            }
        }
    }
}

// 寻找基准点
void find_jidian(uint8_t index[MAX_IMAGE_HEIGHT][MAX_IMAGE_WIDTH]) {
    int mid_w = MT9V03X_W / 2;
    int line = jidian_search_line - 1;

    // 默认值设置
    left_jidian = 1;
    right_jidian = MT9V03X_W - 2;

    // 检查中间区域
    if(index[line][mid_w] == 255 &&
       index[line][mid_w+1] == 255 &&
       index[line][mid_w-1] == 255) {
        // 寻找左边界
        for(uint8_t j = mid_w; j > 0; j--) {
            if(index[line][j-1] == 0 &&
               index[line][j] == 255 &&
               index[line][j+1] == 255) {
                left_jidian = j;
                break;
            }
            if(j-1 == 1) {
                left_jidian = 1;
                break;
            }
        }
        // 寻找右边界
        for(uint8_t j = mid_w; j < MT9V03X_W - 2; j++) {
            if(index[line][j-1] == 255 &&
               index[line][j] == 255 &&
               index[line][j+1] == 0) {
                right_jidian = j;
                break;
            }
            if(j+1 == MT9V03X_W) {
                right_jidian = MT9V03X_W - 2;
                break;
            }
        }
    }
        // 检查左1/4区域
    else if(index[line][MT9V03X_W/4] == 255 &&
            index[line][MT9V03X_W/4+1] == 255 &&
            index[line][MT9V03X_W/4-1] == 255) {
        // 寻找左边界
        for(uint8_t j = MT9V03X_W/4; j > 0; j--) {
            if(index[line][j-1] == 0 &&
               index[line][j] == 255 &&
               index[line][j+1] == 255) {
                left_jidian = j;
                break;
            }
            if(j-1 == 1) {
                left_jidian = 1;
                break;
            }
        }
        // 寻找右边界
        for(uint8_t j = MT9V03X_W/4; j < MT9V03X_W - 2; j++) {
            if(index[line][j-1] == 255 &&
               index[line][j] == 255 &&
               index[line][j+1] == 0) {
                right_jidian = j;
                break;
            }
            if(j+1 == MT9V03X_W) {
                right_jidian = MT9V03X_W - 2;
                break;
            }
        }
    }
        // 检查右3/4区域
    else if(index[line][MT9V03X_W*3/4] == 255 &&
            index[line][MT9V03X_W*3/4+1] == 255 &&
            index[line][MT9V03X_W*3/4-1] == 255) {
        // 寻找左边界
        for(uint8_t j = MT9V03X_W*3/4; j > 0; j--) {
            if(index[line][j-1] == 0 &&
               index[line][j] == 255 &&
               index[line][j+1] == 255) {
                left_jidian = j;
                break;
            }
            if(j-1 == 1) {
                left_jidian = 1;
                break;
            }
        }
        // 寻找右边界
        for(uint8_t j = MT9V03X_W*3/4; j < MT9V03X_W - 2; j++) {
            if(index[line][j-1] == 255 &&
               index[line][j] == 255 &&
               index[line][j+1] == 0) {
                right_jidian = j;
                break;
            }
            if(j+1 == MT9V03X_W) {
                right_jidian = MT9V03X_W - 2;
                break;
            }
        }
    }
}

// 添加一个新的函数用于保存彩色图像
void save_color_image(const char* filename, uint8_t img[MAX_IMAGE_HEIGHT][MAX_IMAGE_WIDTH][3]) {
    // 创建临时一维数组
    uint8_t* temp_img;
    int stride = MT9V03X_W * 3;

    temp_img = (uint8_t*)malloc(MT9V03X_H * stride);
    if (!temp_img) {
        printf("内存分配失败\n");
        return;
    }

    // 正确复制RGB图像数据到一维数组
    for (int i = 0; i < MT9V03X_H; i++) {
        for (int j = 0; j < MT9V03X_W; j++) {
            temp_img[i * stride + j * 3] = img[i][j][0];     // R
            temp_img[i * stride + j * 3 + 1] = img[i][j][1]; // G
            temp_img[i * stride + j * 3 + 2] = img[i][j][2]; // B
        }
    }

    stbi_write_png(filename, MT9V03X_W, MT9V03X_H, 3, temp_img, stride);
    free(temp_img);

    printf("保存彩色图像到: %s\n", filename);
}

// 画线函数
void drawLine(uint8_t img[MAX_IMAGE_HEIGHT][MAX_IMAGE_WIDTH], int x0, int y0, int x1, int y1, uint8_t value) {
    int dx = abs(x1 - x0);
    int dy = abs(y1 - y0);
    int sx = (x0 < x1) ? 1 : -1;
    int sy = (y0 < y1) ? 1 : -1;
    int err = dx - dy;

    while (1) {
        // 确保坐标在图像范围内
        if (x0 >= 0 && x0 < MT9V03X_W && y0 >= 0 && y0 < MT9V03X_H) {
            img[y0][x0] = value;
        }

        if (x0 == x1 && y0 == y1) break;

        int e2 = 2 * err;
        if (e2 > -dy) {
            err -= dy;
            x0 += sx;
        }
        if (e2 < dx) {
            err += dx;
            y0 += sy;
        }
    }
}

// 寻找角点
uint8_t find_corner_point(uint8_t boundary[MAX_IMAGE_HEIGHT], uint8_t threshold) {
    for(int i = 113; i >= 3; i--) { // 从底部向上
        if(abs((int)boundary[i-3] - (int)boundary[i]) > threshold) { // 找到一个角点
            return (uint8_t)i;
        }
    }
    return 255; // 使用255表示未找到
}

// 寻找向上的角点
uint8_t find_corner_point_up(uint8_t boundary[MAX_IMAGE_HEIGHT], uint8_t threshold) {
    for(int i = 20; i < 70; i++) {
        if(abs((int)boundary[i-3] - (int)boundary[i]) > threshold) { // 找到一个角点
            return (uint8_t)i;
        }
    }
    return 255; // 使用255表示未找到
}

// 十字路口边线补全
void crossroad_completions(uint8_t img[MAX_IMAGE_HEIGHT][MAX_IMAGE_WIDTH],
                           uint8_t boundary[MAX_IMAGE_HEIGHT],
                           uint8_t threshold) {
    uint8_t temp_flag_up = find_corner_point_up(boundary, threshold);
    uint8_t temp_flag_down = find_corner_point(boundary, threshold);

    if(temp_flag_up != 255 && temp_flag_down != 255) {
        drawLine(img, boundary[temp_flag_up], temp_flag_up, boundary[temp_flag_down], temp_flag_down, 0);
    }
}

// 处理图像，提取边线
void image_deal(uint8_t index[MAX_IMAGE_HEIGHT][MAX_IMAGE_WIDTH]) {
    uint8_t left_point = left_jidian;
    uint8_t right_point = right_jidian;

    for(uint8_t i = search_start_line - 1; i > search_end_line; i--) {
        uint8_t left_search_judge = 0;      // 左边搜索标志位
        uint8_t mid_start_left_search_judge = 0; // 从中间搜索

        // 寻找左边界点
        for(uint8_t j = left_point; j < left_point + left_line_right_scarch; j++) {
            if(j >= MT9V03X_W - 1) break;

            if(index[i][j-1] == 0 && index[i][j] == 255 && index[i][j+1] == 255) {
                left_point = j;
                break;
            } else if(j == MT9V03X_W - 2) {
                left_point = MT9V03X_W - 5;
                break;
            } else if(j == left_point + left_line_right_scarch - 1) {
                left_search_judge = 1;
                break;
            }
        }

        // 左侧寻找左边界
        if(left_search_judge == 1) {
            for(uint8_t j = left_point; j > left_point - left_line_left_scarch; j--) {
                if(j <= 1 || j >= MT9V03X_W - 1) break;

                if(index[i][j-1] == 0 && index[i][j] == 255 && index[i][j+1] == 255 && j < MT9V03X_W - 5) {
                    left_point = j;
                    break;
                } else if(j == 1) {
                    left_point = 1;
                    mid_start_left_search_judge = 1;
                    break;
                } else if(j == left_point - left_line_left_scarch + 1) {
                    mid_start_left_search_judge = 1;
                    break;
                }
            }
        }

        // 从中间搜索左边界
        if(mid_start_left_search_judge == 1) {
            for(uint8_t j = MID_W; j > 0; j--) {
                if(j <= 1) break;

                if(index[i][j-1] == 0 && index[i][j] == 255 && index[i][j+1] == 255) {
                    left_point = j;
                    break;
                } else if(j == 1) {
                    left_point = 1;
                    break;
                }
            }
        }

        // 右边线
        uint8_t right_search_judge = 0;      // 右边搜索标志位
        uint8_t mid_start_right_search_judge = 0; // 从中间搜索

        // 寻找右边界点
        for(uint8_t j = right_point; j > right_point - right_line_left_scarch; j--) {
            if(j <= 1) break;

            if(index[i][j-1] == 255 && index[i][j] == 255 && index[i][j+1] == 0) {
                right_point = j;
                break;
            } else if(j == 1) {
                right_point = 4;
                break;
            } else if(j == right_point - right_line_left_scarch + 1) {
                right_search_judge = 1;
                break;
            }
        }

        // 向右寻找右边界
        if(right_search_judge == 1) {
            for(uint8_t j = right_point; j < right_point + right_line_right_scarch; j++) {
                if(j >= MT9V03X_W - 1) break;

                if(index[i][j-1] == 255 && index[i][j] == 255 && index[i][j+1] == 0 && j > 4) {
                    right_point = j;
                    break;
                } else if(j == MT9V03X_W - 2) {
                    right_point = MT9V03X_W - 2;
                    mid_start_right_search_judge = 1;
                    break;
                } else if(j == right_point + right_line_right_scarch - 1) {
                    mid_start_right_search_judge = 1;
                    break;
                }
            }
        }

        // 从中间搜索右边界
        if(mid_start_right_search_judge == 1) {
            for(uint8_t j = MID_W; j < MT9V03X_W - 1; j++) {
                if(j >= MT9V03X_W - 1) break;

                if(index[i][j-1] == 255 && index[i][j] == 255 && index[i][j+1] == 0) {
                    right_point = j;
                    break;
                } else if(j == MT9V03X_W - 2) {
                    right_point = MT9V03X_W - 2;
                    break;
                }
            }
        }

        // 保存边线和中线
        left_line_list[i] = Limit_uint8(1, left_point, MT9V03X_W - 2);
        right_line_list[i] = Limit_uint8(1, right_point, MT9V03X_W - 2);
        mid_line_list[i] = Limit_uint8(1, (left_line_list[i] + right_line_list[i]) / 2, MT9V03X_W - 2);
    }

    // 十字路口边线补全
    crossroad_completions(index, left_line_list, 4);
    crossroad_completions(index, right_line_list, 4);

    // 更新中线
    for(int i = 0; i < MT9V03X_H; i++) {
        mid_line_list[i] = (left_line_list[i] + right_line_list[i]) / 2;
    }
}

// 图像边框标记函数
void mark_image_border_optimized(uint8_t img[MAX_IMAGE_HEIGHT][MAX_IMAGE_WIDTH],
                                 uint8_t border_width,
                                 uint8_t border_value) {
    // 参数检查
    if(border_width == 0 ||
       border_width >= MT9V03X_W/2 ||
       border_width >= MT9V03X_H/2) {
        return;
    }

    const int last_col = MT9V03X_W - 1;
    const int last_row = MT9V03X_H - 1;

    // 同时标记四个边
    for(int bw = 0; bw < border_width; bw++) {
        // 上边和下边
        for(int x = bw; x < MT9V03X_W - bw; x++) {
            img[bw][x] = border_value;                  // 上边
            img[last_row - bw][x] = border_value;       // 下边
        }

        // 左边和右边
        for(int y = bw + 1; y < MT9V03X_H - bw - 1; y++) {
            img[y][bw] = border_value;                 // 左边
            img[y][last_col - bw] = border_value;      // 右边
        }
    }
}

// 中线权重列表
uint8_t mid_weight_list[120] = {
        1,1,1,1,1,1,
        1,1,1,1,1,1,1,1,1,1,
        1,1,1,1,1,1,1,1,1,1,
        1,1,1,1,1,1,1,1,1,1,
        1,1,1,1,1,1,1,1,1,1,
        6,6,6,6,6,6,6,6,6,6,
        7,8,9,10,11,12,13,14,15,16,
        17,18,19,20,20,20,20,19,18,17,
        16,15,14,13,12,11,10,9,8,7,
        6,6,6,6,6,6,6,6,6,6,
        1,1,1,1,1,1,1,1,1,1,
        1,1,1,1,1,1,1,1,1,1,
        1,1,1,1,
};

uint8_t last_mid_line = MID_W; // 上次中线值

// 使用权重计算中线
uint8_t find_mid_line_weight(void) {
    uint8_t mid_line_value = MID_W;  // 最终中线参考值
    uint8_t mid_line = MID_W;        // 中线暂存值
    uint32_t weight_midline_sum = 0; // 加权中线值
    uint32_t weight_sum = 0;         // 权重累计值

    for(uint8_t i = MT9V03X_H - 1; i > search_end_line; i--) {
        weight_midline_sum += mid_line_list[i] * mid_weight_list[i];
        weight_sum += mid_weight_list[i];
    }

    mid_line = (uint8_t)(weight_midline_sum / weight_sum);
    mid_line_value = Limit_int(MID_W - mid_line_limit, last_mid_line * 0.2 + mid_line * 0.8, MID_W + mid_line_limit); // 滑动滤波
    last_mid_line = mid_line_value;

    return mid_line_value;
}

// 创建彩色显示图像
void create_display_image() {
    // 首先将二值图转换到RGB
    for(int i = 0; i < MT9V03X_H; i++) {
        for(int j = 0; j < MT9V03X_W; j++) {
            display_image[i][j][0] = image[i][j];
            display_image[i][j][1] = image[i][j];
            display_image[i][j][2] = image[i][j];
        }
    }

    // 绘制左边线 - 红色
    for(uint8_t i = MT9V03X_H - 1; i > search_end_line; i--) {
        int x = Limit_int(1, left_line_list[i], MT9V03X_W - 2);
        display_image[i][x][0] = 255;
        display_image[i][x][1] = 0;
        display_image[i][x][2] = 0;
    }

    // 绘制右边线 - 绿色
    for(uint8_t i = MT9V03X_H - 1; i > search_end_line; i--) {
        int x = Limit_int(1, right_line_list[i], MT9V03X_W - 2);
        display_image[i][x][0] = 0;
        display_image[i][x][1] = 255;
        display_image[i][x][2] = 0;
    }

    // 绘制中线 - 蓝色
    for(uint8_t i = MT9V03X_H - 1; i > search_end_line; i--) {
        int x = Limit_int(1, mid_line_list[i], MT9V03X_W - 2);
        display_image[i][x][0] = 0;
        display_image[i][x][1] = 0;
        display_image[i][x][2] = 255;
    }
}

int main(int argc, char *argv[]) {
    if(argc < 3) {
        printf("用法: %s <输入图像路径> <输出图像路径前缀>\n", argv[0]);
        return 1;
    }

    // 读取输入图像
    int width, height, channels;
    uint8_t *input_image = stbi_load(argv[1], &width, &height, &channels, 1); // 强制转换为灰度图

    if(!input_image) {
        printf("无法读取输入图像: %s\n", argv[1]);
        return 1;
    }

    printf("成功读取图像: 宽度=%d, 高度=%d, 通道数=%d\n", width, height, channels);

    // 设置图像尺寸
    MT9V03X_W = width;
    MT9V03X_H = height;

    if(MT9V03X_W > MAX_IMAGE_WIDTH || MT9V03X_H > MAX_IMAGE_HEIGHT) {
        printf("图像尺寸过大, 请不要超过 %dx%d\n", MAX_IMAGE_WIDTH, MAX_IMAGE_HEIGHT);
        stbi_image_free(input_image);
        return 1;
    }

    // 复制输入图像到处理缓冲区
    for(int i = 0; i < MT9V03X_H; i++) {
        for(int j = 0; j < MT9V03X_W; j++) {
            base_image[i][j] = input_image[i * MT9V03X_W + j];
        }
    }

    // 释放stb_image分配的内存
    stbi_image_free(input_image);

    // 获取输出文件名前缀（不包含扩展名）
    char output_prefix[256];
    strcpy(output_prefix, argv[2]);
    char *dot = strrchr(output_prefix, '.');
    if (dot) *dot = '\0';  // 去掉扩展名

    // 保存原始灰度图
    save_stage_image(output_prefix, "1_original", base_image, 0);

    // 图像处理流程
    printf("计算Otsu阈值...\n");
    img_threshold = Ostu(base_image);
    printf("Otsu阈值: %d\n", img_threshold);

    printf("二值化图像...\n");
    set_image_twovalues(img_threshold);
    // 保存二值化图像
    save_stage_image(output_prefix, "2_binary", image, 1);

    printf("寻找基准点...\n");
    find_jidian(image);
    printf("基准点: 左=%d, 右=%d\n", left_jidian, right_jidian);

    printf("标记图像边框...\n");
    mark_image_border_optimized(image, 1, 0);

    printf("处理边线...\n");
    image_deal(image);

    // 创建包含左右边线的图像并保存
    create_display_image();
    // 保存只有边线的图像
    char borders_path[256];
    sprintf(borders_path, "%s_3_borders.png", output_prefix);
    save_color_image(borders_path, display_image);
//    stbi_write_png(borders_path, MT9V03X_W, MT9V03X_H, 3, display_image, MT9V03X_W * 3);
    printf("保存边线图像到: %s\n", borders_path);

    // 计算中线...
    printf("计算中线...\n");
    final_mid_line = find_mid_line_weight();
    printf("最终中线位置: %d\n", final_mid_line);

    // 创建最终显示图像
    create_display_image();

    // 保存最终结果（包含中线）
    char final_path[256];
    sprintf(final_path, "%s_4_final.png", output_prefix);
    save_color_image(final_path, display_image);
//    stbi_write_png(final_path, MT9V03X_W, MT9V03X_H, 3, display_image, MT9V03X_W * 3);
    printf("保存最终结果图像到: %s\n", final_path);

    printf("处理完成!\n");
    return 0;
}