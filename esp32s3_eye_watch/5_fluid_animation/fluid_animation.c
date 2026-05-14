/*
 * 5_fluid_animation - 流体动画效果模块
 * @brief 流体粒子模拟、动画渲染、定时器控制
 * 
 * 本模块提供：
 * - 流体粒子初始化
 * - 粒子物理模拟（重力、摩擦、碰撞）
 * - 粒子渲染到 Canvas
 * - 动画定时器
 * 
 * 重力方向由 gravity_control 模块提供（参考 display_rotation 实现）
 */

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "lvgl.h"
#include "esp_log.h"
#include "esp_timer.h"

#include "gravity_control.h"

static const char *TAG = "fluid_animation";

// 流体参数
#define FLUID_WIDTH 160
#define FLUID_HEIGHT 160
#define FLUID_NUM_PARTICLES 150

// 粒子数据
static float *g_px = NULL;
static float *g_py = NULL;
static float *g_pvx = NULL;
static float *g_pvy = NULL;
static uint8_t *g_fluid_buf = NULL;
static lv_obj_t *g_fluid_canvas = NULL;
static lv_timer_t *g_fluid_timer = NULL;

// 重力向量（从 gravity_control 模块获取）
static float g_gravity_x = 0.0f;
static float g_gravity_y = 0.08f;

// 初始化流体
void fluid_init(void)
{
    // 释放已有内存
    if (g_px) free(g_px);
    if (g_py) free(g_py);
    if (g_pvx) free(g_pvx);
    if (g_pvy) free(g_pvy);
    if (g_fluid_buf) free(g_fluid_buf);

    // 分配粒子内存
    g_px = (float *)malloc(FLUID_NUM_PARTICLES * sizeof(float));
    g_py = (float *)malloc(FLUID_NUM_PARTICLES * sizeof(float));
    g_pvx = (float *)malloc(FLUID_NUM_PARTICLES * sizeof(float));
    g_pvy = (float *)malloc(FLUID_NUM_PARTICLES * sizeof(float));
    g_fluid_buf = (uint8_t *)malloc(FLUID_WIDTH * FLUID_HEIGHT * 2);

    if (!g_px || !g_py || !g_pvx || !g_pvy || !g_fluid_buf) {
        ESP_LOGE(TAG, "Fluid memory allocation failed");
        return;
    }

    // 初始化粒子位置（随机分布）
    srand(esp_timer_get_time());
    int border = 10;
    for (int i = 0; i < FLUID_NUM_PARTICLES; i++) {
        g_px[i] = (float)(border + (rand() % (FLUID_WIDTH - 2 * border)));
        g_py[i] = (float)(border + (rand() % (FLUID_HEIGHT - 2 * border)));
        g_pvx[i] = ((float)(rand() % 200) - 100.0f) / 20.0f;
        g_pvy[i] = ((float)(rand() % 100) - 50.0f) / 20.0f;
    }
    
    ESP_LOGI(TAG, "Fluid initialized with %d particles", FLUID_NUM_PARTICLES);
}

// 更新流体物理
void fluid_update(void)
{
    float friction = 0.98f;
    float bounce = 0.6f;
    
    // 从 gravity_control 模块获取当前重力向量（参考 display_rotation）
    gravity_control_get_vector(&g_gravity_x, &g_gravity_y);

    for (int i = 0; i < FLUID_NUM_PARTICLES; i++) {
        // 应用摩擦
        g_pvx[i] *= friction;
        g_pvy[i] *= friction;

        // 应用重力（从 gravity_control 模块获取）
        g_pvx[i] += g_gravity_x;
        g_pvy[i] += g_gravity_y;

        // 粒子相互作用（简单 FIP 行为）
        for (int j = 0; j < FLUID_NUM_PARTICLES; j++) {
            if (i != j) {
                float dx = g_px[j] - g_px[i];
                float dy = g_py[j] - g_py[i];
                float dist = sqrtf(dx * dx + dy * dy);

                if (dist < 15.0f && dist > 0.1f) {
                    float force = (15.0f - dist) / 15.0f;
                    g_pvx[i] -= dx / dist * force * 0.1f;
                    g_pvy[i] -= dy / dist * force * 0.1f;
                }
            }
        }

        // 更新位置
        g_px[i] += g_pvx[i];
        g_py[i] += g_pvy[i];

        // 边界碰撞
        if (g_px[i] < 5) {
            g_px[i] = 5;
            g_pvx[i] = -g_pvx[i] * bounce;
        }
        if (g_px[i] >= FLUID_WIDTH - 5) {
            g_px[i] = FLUID_WIDTH - 6;
            g_pvx[i] = -g_pvx[i] * bounce;
        }
        if (g_py[i] < 5) {
            g_py[i] = 5;
            g_pvy[i] = -g_pvy[i] * bounce;
        }
        if (g_py[i] >= FLUID_HEIGHT - 5) {
            g_py[i] = FLUID_HEIGHT - 6;
            g_pvy[i] = -g_pvy[i] * bounce;
        }
    }
}

// 绘制流体
void fluid_draw(void)
{
    if (!g_fluid_buf || !g_fluid_canvas) return;

    // 清空缓冲区（深色背景）
    for (int i = 0; i < FLUID_WIDTH * FLUID_HEIGHT; i++) {
        g_fluid_buf[i * 2] = 0x0A;
        g_fluid_buf[i * 2 + 1] = 0x00;
    }

    // 绘制粒子（多种颜色）
    for (int i = 0; i < FLUID_NUM_PARTICLES; i++) {
        int x = (int)g_px[i];
        int y = (int)g_py[i];

        if (x < 4 || x >= FLUID_WIDTH - 4 || y < 4 || y >= FLUID_HEIGHT - 4) {
            continue;
        }

        // 交替粒子颜色（青色、白色、黄色）
        uint16_t particle_color;
        if (i % 3 == 0) {
            particle_color = 0x07FF;  // Cyan
        } else if (i % 3 == 1) {
            particle_color = 0xFFFF;  // White
        } else {
            particle_color = 0xFFE0;  // Yellow
        }

        // 绘制 3x3 粒子方块
        for (int dy = -2; dy <= 2; dy++) {
            for (int dx = -2; dx <= 2; dx++) {
                int px = x + dx;
                int py = y + dy;
                if (px >= 0 && px < FLUID_WIDTH && py >= 0 && py < FLUID_HEIGHT) {
                    int idx = py * FLUID_WIDTH + px;
                    g_fluid_buf[idx * 2] = particle_color & 0xFF;
                    g_fluid_buf[idx * 2 + 1] = particle_color >> 8;
                }
            }
        }
    }

    // 绘制边框（金色）
    uint16_t border_color = 0xFBE0;
    int bw = 3;
    for (int y = 0; y < FLUID_HEIGHT; y++) {
        for (int x = 0; x < bw; x++) {
            int idx1 = y * FLUID_WIDTH + x;
            int idx2 = y * FLUID_WIDTH + (FLUID_WIDTH - 1 - x);
            g_fluid_buf[idx1 * 2] = border_color & 0xFF;
            g_fluid_buf[idx1 * 2 + 1] = border_color >> 8;
            g_fluid_buf[idx2 * 2] = border_color & 0xFF;
            g_fluid_buf[idx2 * 2 + 1] = border_color >> 8;
        }
    }
    for (int x = 0; x < FLUID_WIDTH; x++) {
        for (int y = 0; y < bw; y++) {
            int idx1 = y * FLUID_WIDTH + x;
            int idx2 = (FLUID_HEIGHT - 1 - y) * FLUID_WIDTH + x;
            g_fluid_buf[idx1 * 2] = border_color & 0xFF;
            g_fluid_buf[idx1 * 2 + 1] = border_color >> 8;
            g_fluid_buf[idx2 * 2] = border_color & 0xFF;
            g_fluid_buf[idx2 * 2 + 1] = border_color >> 8;
        }
    }

    lv_obj_invalidate(g_fluid_canvas);
}

// 定时器回调
static void fluid_timer_cb(lv_timer_t *timer)
{
    fluid_update();
    fluid_draw();
}

// 创建流体 UI
void fluid_create_ui(lv_obj_t *scr)
{
    ESP_LOGI(TAG, "Creating fluid UI");
    
    fluid_init();
    
    if (!g_px || !g_py || !g_pvx || !g_pvy || !g_fluid_buf) {
        ESP_LOGE(TAG, "Fluid memory allocation failed!");
        return;
    }
    
    // 创建 Canvas
    g_fluid_canvas = lv_canvas_create(scr);
    if (!g_fluid_canvas) {
        ESP_LOGE(TAG, "Canvas creation failed!");
        return;
    }
    
    lv_obj_center(g_fluid_canvas);
    lv_canvas_set_buffer(g_fluid_canvas, g_fluid_buf, FLUID_WIDTH, FLUID_HEIGHT, LV_COLOR_FORMAT_RGB565);
    lv_obj_set_style_bg_color(g_fluid_canvas, lv_color_hex(0xFFD700), 0);
    lv_obj_set_style_border_width(g_fluid_canvas, 3, 0);
    lv_obj_set_style_border_color(g_fluid_canvas, lv_color_hex(0xFFD700), 0);
    
    // 创建定时器
    if (g_fluid_timer) {
        lv_timer_del(g_fluid_timer);
    }
    g_fluid_timer = lv_timer_create(fluid_timer_cb, 50, NULL);
    
    // 立即绘制第一帧
    fluid_draw();
    
    ESP_LOGI(TAG, "Fluid UI created with %d particles", FLUID_NUM_PARTICLES);
}

// 销毁流体 UI
void fluid_destroy_ui(void)
{
    if (g_fluid_timer) {
        lv_timer_del(g_fluid_timer);
        g_fluid_timer = NULL;
    }
    
    if (g_px) { free(g_px); g_px = NULL; }
    if (g_py) { free(g_py); g_py = NULL; }
    if (g_pvx) { free(g_pvx); g_pvx = NULL; }
    if (g_pvy) { free(g_pvy); g_pvy = NULL; }
    if (g_fluid_buf) { free(g_fluid_buf); g_fluid_buf = NULL; }
    
    g_fluid_canvas = NULL;
    
    ESP_LOGI(TAG, "Fluid UI destroyed");
}

// 获取当前重力向量（供外部查询）
void fluid_get_gravity(float *gx, float *gy)
{
    if (gx) *gx = g_gravity_x;
    if (gy) *gy = g_gravity_y;
}

// 更新重力向量（从 gravity_control 模块同步）
void fluid_update_gravity_from_control(void)
{
    gravity_control_get_vector(&g_gravity_x, &g_gravity_y);
    ESP_LOGD(TAG, "Gravity synced from control: (%.2f, %.2f)", g_gravity_x, g_gravity_y);
}