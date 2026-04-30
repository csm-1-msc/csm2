/*
 * SPDX-FileCopyrightText: 2021-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "esp_log.h"
#include "lvgl.h"

static const char *TAG = "watch_ui";

/**
 * @brief Step 2: Key Driver
 * @details Add time update with timer and basic watch UI
 * - Time updates every second
 * - Basic watch UI with border and labels
 * - No color switching yet (placeholder for Step 3)
 */

static time_t g_current_ts = 0;
static lv_timer_t *g_watch_timer = NULL;

static lv_obj_t *g_time_label = NULL;
static lv_obj_t *g_date_label = NULL;
static lv_obj_t *g_weekday_label = NULL;

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
    if (g_time_label && g_date_label && g_weekday_label) {
        g_current_ts++;
        update_labels();
    }
}

static void create_watch_ui(lv_obj_t *scr)
{
    lv_obj_clean(scr);
    lv_obj_set_style_bg_color(scr, lv_color_hex(0x000000), 0);

    // Main frame - blue rounded rectangle with dark blue background
    lv_obj_t *main_frame = lv_obj_create(scr);
    lv_obj_set_size(main_frame, 220, 220);
    lv_obj_center(main_frame);
    lv_obj_set_style_bg_color(main_frame, lv_color_hex(0x0a0a2a), 0);
    lv_obj_set_style_border_width(main_frame, 3, 0);
    lv_obj_set_style_border_color(main_frame, lv_color_hex(0x00AFFF), 0);
    lv_obj_set_style_radius(main_frame, 15, 0);
    lv_obj_set_style_pad_all(main_frame, 0, 0);

    // Top title: ESP32-S3-EYE
    lv_obj_t *title = lv_label_create(main_frame);
    lv_label_set_text(title, "ESP32-S3-EYE");
    lv_obj_set_style_text_color(title, lv_color_hex(0x00AFFF), 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_14, 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 15);

    // Center big time: HH:MM:SS
    g_time_label = lv_label_create(main_frame);
    lv_label_set_text(g_time_label, "12:00:00");
    lv_obj_set_style_text_color(g_time_label, lv_color_hex(0x00AFFF), 0);
    lv_obj_set_style_text_font(g_time_label, &lv_font_montserrat_14, 0);
    lv_obj_align(g_time_label, LV_ALIGN_CENTER, 0, -10);

    // Date: YYYY MM DD
    g_date_label = lv_label_create(main_frame);
    lv_label_set_text(g_date_label, "2026 04 08");
    lv_obj_set_style_text_color(g_date_label, lv_color_hex(0x00AFFF), 0);
    lv_obj_set_style_text_font(g_date_label, &lv_font_montserrat_14, 0);
    lv_obj_align(g_date_label, LV_ALIGN_CENTER, 0, 25);

    // Weekday: "3"
    g_weekday_label = lv_label_create(main_frame);
    lv_label_set_text(g_weekday_label, "3");
    lv_obj_set_style_text_color(g_weekday_label, lv_color_hex(0x00AFFF), 0);
    lv_obj_set_style_text_font(g_weekday_label, &lv_font_montserrat_14, 0);
    lv_obj_align(g_weekday_label, LV_ALIGN_CENTER, 0, 45);

    // Bottom left dot
    lv_obj_t *dot_left = lv_obj_create(main_frame);
    lv_obj_set_size(dot_left, 10, 10);
    lv_obj_align(dot_left, LV_ALIGN_BOTTOM_LEFT, 25, -15);
    lv_obj_set_style_bg_color(dot_left, lv_color_hex(0x00AFFF), 0);
    lv_obj_set_style_border_width(dot_left, 0, 0);
    lv_obj_set_style_radius(dot_left, 5, 0);

    // Bottom right dot
    lv_obj_t *dot_right = lv_obj_create(main_frame);
    lv_obj_set_size(dot_right, 10, 10);
    lv_obj_align(dot_right, LV_ALIGN_BOTTOM_RIGHT, -25, -15);
    lv_obj_set_style_bg_color(dot_right, lv_color_hex(0x00AFFF), 0);
    lv_obj_set_style_border_width(dot_right, 0, 0);
    lv_obj_set_style_radius(dot_right, 5, 0);

    update_labels();

    // Create/update timer - delete old timer first
    if (g_watch_timer) {
        lv_timer_del(g_watch_timer);
        g_watch_timer = NULL;
    }
    g_watch_timer = lv_timer_create(timer_cb, 1000, NULL);

    ESP_LOGI(TAG, "Step 2: Watch UI with time update created");
}

void example_lvgl_demo_ui(lv_obj_t *scr)
{
    init_time();
    create_watch_ui(scr);
}

// Placeholder for Step 3: Color switching
void watch_switch_style(void)
{
    ESP_LOGI(TAG, "Step 2: Color switching not implemented yet (Step 3)");
}

void watch_switch_ui(void)
{
    ESP_LOGI(TAG, "Step 2: UI switching not implemented yet");
}