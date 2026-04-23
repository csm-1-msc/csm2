/*
 * SPDX-FileCopyrightText: 2021-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h>

#include "esp_log.h"
#include "esp_timer.h"
#include "lvgl.h"

#ifndef LV_IMG_CF_TRUE_COLOR
#define LV_IMG_CF_TRUE_COLOR 0x05
#endif

static const char *TAG = "watch_ui";

typedef enum {
    STYLE_INITIAL = 0,    // 初始状态：默认颜色
    STYLE_COLOR1,         // 第 1 次按键：颜色 1
    STYLE_COLOR2,         // 第 2 次按键：颜色 2
    STYLE_COLOR3,         // 第 3 次按键：颜色 3
    STYLE_FLUID           // 第 4 次按键：流体动画
} watch_style_t;

static watch_style_t g_watch_style = STYLE_INITIAL;
static time_t g_current_ts = 0;

static lv_timer_t *g_watch_timer = NULL;
static lv_timer_t *g_fluid_timer = NULL;

static lv_obj_t *g_time_label = NULL;
static lv_obj_t *g_date_label = NULL;
static lv_obj_t *g_weekday_label = NULL;

// Flag to track if we're in fluid mode
static bool g_in_fluid_mode = false;

// Watch style colors: 4 colors for watch UI (style 4 is fluid animation)
static const struct {
    uint32_t primary;
    uint32_t bg;
} STYLE_COLORS[4] = {
    {0x00AFFF, 0x0a0a2a},    // STYLE_INITIAL: Modern: Blue
    {0xFFD700, 0x2a1a00},    // STYLE_COLOR1: Classic: Gold
    {0xFFFFFF, 0x0a0a0a},    // STYLE_COLOR2: Minimal: White/Black
    {0x00FF00, 0x001a0a}     // STYLE_COLOR3: Digital: Green
};

// Fluid simulation parameters - reduced for ESP32-S3-EYE memory constraints
#define FLUID_WIDTH 160
#define FLUID_HEIGHT 160
#define FLUID_NUM_PARTICLES 200

static float *g_px = NULL;
static float *g_py = NULL;
static float *g_pvx = NULL;
static float *g_pvy = NULL;
static uint8_t *g_fluid_buf = NULL;
static lv_obj_t *g_fluid_canvas = NULL;

static void init_time(void)
{
    // Fixed start time: 2026-04-08 12:00:00
    struct tm timeinfo = {0};
    timeinfo.tm_year = 126;  // 2026 - 1900
    timeinfo.tm_mon = 3;     // April (0-11)
    timeinfo.tm_mday = 8;
    timeinfo.tm_hour = 12;
    timeinfo.tm_min = 0;
    timeinfo.tm_sec = 0;
    g_current_ts = mktime(&timeinfo);
}

static void update_labels(void)
{
    struct tm timeinfo;
    localtime_r(&g_current_ts, &timeinfo);

    char buf[32];

    // Time: HH:MM:SS (24-hour format)
    snprintf(buf, sizeof(buf), "%02d:%02d:%02d",
             timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
    if (g_time_label) lv_label_set_text(g_time_label, buf);

    // Date: YYYY MM DD
    snprintf(buf, sizeof(buf), "2026 04 08");
    if (g_date_label) lv_label_set_text(g_date_label, buf);

    // Weekday: fixed as "3" (Wednesday)
    snprintf(buf, sizeof(buf), "3");
    if (g_weekday_label) lv_label_set_text(g_weekday_label, buf);
}

static void timer_cb(lv_timer_t *timer)
{
    (void)timer;
    // Only update labels if not in fluid mode and all labels exist
    // Double-check: also verify labels are still valid (not deleted)
    if (!g_in_fluid_mode && g_time_label && g_date_label && g_weekday_label) {
        // Additional check: verify labels are still part of a valid object tree
        if (lv_obj_get_parent(g_time_label) != NULL && 
            lv_obj_get_parent(g_date_label) != NULL && 
            lv_obj_get_parent(g_weekday_label) != NULL) {
            g_current_ts++;
            update_labels();
        }
    }
}

static void create_watch_ui(lv_obj_t *scr)
{
    g_in_fluid_mode = false;
    
    lv_obj_clean(scr);
    lv_obj_set_style_bg_color(scr, lv_color_hex(0x000000), 0);

    uint32_t color = STYLE_COLORS[g_watch_style].primary;
    uint32_t bg_color = STYLE_COLORS[g_watch_style].bg;

    // Main frame - blue rounded rectangle with dark blue background
    lv_obj_t *main_frame = lv_obj_create(scr);
    lv_obj_set_size(main_frame, 220, 220);
    lv_obj_center(main_frame);
    lv_obj_set_style_bg_color(main_frame, lv_color_hex(bg_color), 0);
    lv_obj_set_style_border_width(main_frame, 3, 0);
    lv_obj_set_style_border_color(main_frame, lv_color_hex(color), 0);
    lv_obj_set_style_radius(main_frame, 15, 0);
    lv_obj_set_style_pad_all(main_frame, 0, 0);

    // Top title: ESP32-S3-EYE
    lv_obj_t *title = lv_label_create(main_frame);
    lv_label_set_text(title, "ESP32-S3-EYE");
    lv_obj_set_style_text_color(title, lv_color_hex(color), 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_14, 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 15);

    // Center big time: HH:MM:SS
    g_time_label = lv_label_create(main_frame);
    lv_label_set_text(g_time_label, "12:00:00");
    lv_obj_set_style_text_color(g_time_label, lv_color_hex(color), 0);
    lv_obj_set_style_text_font(g_time_label, &lv_font_montserrat_14, 0);
    lv_obj_align(g_time_label, LV_ALIGN_CENTER, 0, -10);

    // Date: YYYY MM DD
    g_date_label = lv_label_create(main_frame);
    lv_label_set_text(g_date_label, "2026 04 08");
    lv_obj_set_style_text_color(g_date_label, lv_color_hex(color), 0);
    lv_obj_set_style_text_font(g_date_label, &lv_font_montserrat_14, 0);
    lv_obj_align(g_date_label, LV_ALIGN_CENTER, 0, 25);

    // Weekday: "3"
    g_weekday_label = lv_label_create(main_frame);
    lv_label_set_text(g_weekday_label, "3");
    lv_obj_set_style_text_color(g_weekday_label, lv_color_hex(color), 0);
    lv_obj_set_style_text_font(g_weekday_label, &lv_font_montserrat_14, 0);
    lv_obj_align(g_weekday_label, LV_ALIGN_CENTER, 0, 45);

    // Bottom left dot
    lv_obj_t *dot_left = lv_obj_create(main_frame);
    lv_obj_set_size(dot_left, 10, 10);
    lv_obj_align(dot_left, LV_ALIGN_BOTTOM_LEFT, 25, -15);
    lv_obj_set_style_bg_color(dot_left, lv_color_hex(color), 0);
    lv_obj_set_style_border_width(dot_left, 0, 0);
    lv_obj_set_style_radius(dot_left, 5, 0);

    // Bottom right dot
    lv_obj_t *dot_right = lv_obj_create(main_frame);
    lv_obj_set_size(dot_right, 10, 10);
    lv_obj_align(dot_right, LV_ALIGN_BOTTOM_RIGHT, -25, -15);
    lv_obj_set_style_bg_color(dot_right, lv_color_hex(color), 0);
    lv_obj_set_style_border_width(dot_right, 0, 0);
    lv_obj_set_style_radius(dot_right, 5, 0);

    update_labels();

    // Create/update timer - delete old timer first
    if (g_watch_timer) {
        lv_timer_del(g_watch_timer);
        g_watch_timer = NULL;
    }
    g_watch_timer = lv_timer_create(timer_cb, 1000, NULL);
}

static void fluid_init(void)
{
    // Free existing memory if any
    if (g_px) free(g_px);
    if (g_py) free(g_py);
    if (g_pvx) free(g_pvx);
    if (g_pvy) free(g_pvy);
    if (g_fluid_buf) free(g_fluid_buf);

    // Allocate memory for 500 particles
    g_px = (float *)malloc(FLUID_NUM_PARTICLES * sizeof(float));
    g_py = (float *)malloc(FLUID_NUM_PARTICLES * sizeof(float));
    g_pvx = (float *)malloc(FLUID_NUM_PARTICLES * sizeof(float));
    g_pvy = (float *)malloc(FLUID_NUM_PARTICLES * sizeof(float));
    g_fluid_buf = (uint8_t *)malloc(FLUID_WIDTH * FLUID_HEIGHT * 3);

    if (!g_px || !g_py || !g_pvx || !g_pvy || !g_fluid_buf) {
        ESP_LOGE(TAG, "Fluid memory allocation failed");
        return;
    }

    // Initialize particles with random positions
    srand(esp_timer_get_time());
    for (int i = 0; i < FLUID_NUM_PARTICLES; i++) {
        g_px[i] = (float)(rand() % FLUID_WIDTH);
        g_py[i] = (float)(rand() % (FLUID_HEIGHT / 2));
        g_pvx[i] = ((float)(rand() % 100) - 50.0f) / 50.0f;
        g_pvy[i] = 0;
    }
}

static void fluid_update(void)
{
    float gravity = 0.08f;
    float friction = 0.98f;
    float bounce = 0.6f;

    for (int i = 0; i < FLUID_NUM_PARTICLES; i++) {
        // Apply friction
        g_pvx[i] *= friction;
        g_pvy[i] *= friction;

        // Apply gravity
        g_pvy[i] += gravity;

        // Particle interaction (simple FIP-like behavior)
        for (int j = 0; j < FLUID_NUM_PARTICLES; j++) {
            if (i != j) {
                float dx = g_px[j] - g_px[i];
                float dy = g_py[j] - g_py[i];
                float dist = sqrt(dx * dx + dy * dy);

                if (dist < 15.0f && dist > 0.1f) {
                    float force = (15.0f - dist) / 15.0f;
                    g_pvx[i] -= dx / dist * force * 0.1f;
                    g_pvy[i] -= dy / dist * force * 0.1f;
                }
            }
        }

        // Update position
        g_px[i] += g_pvx[i];
        g_py[i] += g_pvy[i];

        // Boundary collision
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

static void fluid_draw(void)
{
    if (!g_fluid_buf) return;

    // Clear buffer to dark background
    memset(g_fluid_buf, 10, FLUID_WIDTH * FLUID_HEIGHT * 3);

    // Draw particles
    for (int i = 0; i < FLUID_NUM_PARTICLES; i++) {
        int x = (int)g_px[i];
        int y = (int)g_py[i];

        if (x >= 0 && x < FLUID_WIDTH && y >= 0 && y < FLUID_HEIGHT) {
            // Draw a small circle for each particle
            for (int dy = -2; dy <= 2; dy++) {
                for (int dx = -2; dx <= 2; dx++) {
                    int px = x + dx;
                    int py = y + dy;
                    if (px >= 0 && px < FLUID_WIDTH && py >= 0 && py < FLUID_HEIGHT) {
                        if (dx * dx + dy * dy <= 4) {
                            int idx = (py * FLUID_WIDTH + px) * 3;
                            g_fluid_buf[idx] = 0;
                            g_fluid_buf[idx + 1] = 200;
                            g_fluid_buf[idx + 2] = 255;
                        }
                    }
                }
            }
        }
    }

    if (g_fluid_canvas) {
        lv_canvas_set_buffer(g_fluid_canvas, g_fluid_buf, FLUID_WIDTH, FLUID_HEIGHT, LV_IMG_CF_TRUE_COLOR);
    }
}

static void fluid_timer_cb(lv_timer_t *timer)
{
    (void)timer;
    fluid_update();
    fluid_draw();
}

static void create_fluid_ui(lv_obj_t *scr)
{
    // Stop watch timer FIRST before cleaning screen
    if (g_watch_timer) {
        lv_timer_del(g_watch_timer);
        g_watch_timer = NULL;
    }
    g_in_fluid_mode = true;
    
    lv_obj_clean(scr);
    lv_obj_set_style_bg_color(scr, lv_color_hex(0x0a0a1a), 0);

    fluid_init();

    // Create canvas with explicit parent
    g_fluid_canvas = lv_canvas_create(scr);
    lv_canvas_set_buffer(g_fluid_canvas, g_fluid_buf, FLUID_WIDTH, FLUID_HEIGHT, LV_IMG_CF_TRUE_COLOR);
    lv_obj_align(g_fluid_canvas, LV_ALIGN_CENTER, 0, 0);

    // Delete old fluid timer first
    if (g_fluid_timer) {
        lv_timer_del(g_fluid_timer);
        g_fluid_timer = NULL;
    }
    g_fluid_timer = lv_timer_create(fluid_timer_cb, 33, NULL);
    
    ESP_LOGI(TAG, "Fluid UI created with %d particles", FLUID_NUM_PARTICLES);
}

static void destroy_fluid_ui(void)
{
    if (g_fluid_timer) {
        lv_timer_del(g_fluid_timer);
        g_fluid_timer = NULL;
    }
    g_in_fluid_mode = false;
    if (g_px) { free(g_px); g_px = NULL; }
    if (g_py) { free(g_py); g_py = NULL; }
    if (g_pvx) { free(g_pvx); g_pvx = NULL; }
    if (g_pvy) { free(g_pvy); g_pvy = NULL; }
    if (g_fluid_buf) { free(g_fluid_buf); g_fluid_buf = NULL; }
    g_fluid_canvas = NULL;
}

void example_lvgl_demo_ui(lv_obj_t *scr)
{
    g_watch_style = STYLE_INITIAL;
    init_time();
    create_watch_ui(scr);
}

void watch_switch_style(void)
{
    lv_obj_t *scr = lv_disp_get_scr_act(NULL);
    
    // 5 段循环：初始→颜色 1→颜色 2→颜色 3→流体→初始→...
    switch (g_watch_style) {
        case STYLE_INITIAL:
            g_watch_style = STYLE_COLOR1;
            ESP_LOGI(TAG, "Switch to STYLE_COLOR1");
            create_watch_ui(scr);
            break;
        case STYLE_COLOR1:
            g_watch_style = STYLE_COLOR2;
            ESP_LOGI(TAG, "Switch to STYLE_COLOR2");
            create_watch_ui(scr);
            break;
        case STYLE_COLOR2:
            g_watch_style = STYLE_COLOR3;
            ESP_LOGI(TAG, "Switch to STYLE_COLOR3");
            create_watch_ui(scr);
            break;
        case STYLE_COLOR3:
            g_watch_style = STYLE_FLUID;
            ESP_LOGI(TAG, "Switch to STYLE_FLUID");
            create_fluid_ui(scr);
            break;
        case STYLE_FLUID:
            g_watch_style = STYLE_INITIAL;
            ESP_LOGI(TAG, "Switch to STYLE_INITIAL");
            destroy_fluid_ui();
            // Re-initialize time when switching back to watch
            init_time();
            create_watch_ui(scr);
            break;
    }
}

void watch_switch_ui(void)
{
    // This function is deprecated, use watch_switch_style instead
    watch_switch_style();
}
