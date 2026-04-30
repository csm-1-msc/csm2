/*
 * SPDX-FileCopyrightText: 2021-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "esp_log.h"
#include "lvgl.h"

static const char *TAG = "watch_ui";

/**
 * @brief Step 1: Project Base
 * @details Basic LVGL UI with static time display
 * - Simple watch UI with border and labels
 * - No animation, no color switching, no fluid mode
 */

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

    // Center big time: Fixed time
    lv_obj_t *time_label = lv_label_create(main_frame);
    lv_label_set_text(time_label, "12:00:00");
    lv_obj_set_style_text_color(time_label, lv_color_hex(0x00AFFF), 0);
    lv_obj_set_style_text_font(time_label, &lv_font_montserrat_14, 0);
    lv_obj_align(time_label, LV_ALIGN_CENTER, 0, -10);

    // Date: Fixed date
    lv_obj_t *date_label = lv_label_create(main_frame);
    lv_label_set_text(date_label, "2026 04 08");
    lv_obj_set_style_text_color(date_label, lv_color_hex(0x00AFFF), 0);
    lv_obj_set_style_text_font(date_label, &lv_font_montserrat_14, 0);
    lv_obj_align(date_label, LV_ALIGN_CENTER, 0, 25);

    // Weekday: Fixed
    lv_obj_t *weekday_label = lv_label_create(main_frame);
    lv_label_set_text(weekday_label, "3");
    lv_obj_set_style_text_color(weekday_label, lv_color_hex(0x00AFFF), 0);
    lv_obj_set_style_text_font(weekday_label, &lv_font_montserrat_14, 0);
    lv_obj_align(weekday_label, LV_ALIGN_CENTER, 0, 45);

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

    ESP_LOGI(TAG, "Step 1: Basic watch UI created");
}

void example_lvgl_demo_ui(lv_obj_t *scr)
{
    create_watch_ui(scr);
}

// Placeholder functions for future steps
void watch_switch_style(void)
{
    ESP_LOGI(TAG, "Step 1: Color switching not implemented yet");
}

void watch_switch_ui(void)
{
    ESP_LOGI(TAG, "Step 1: UI switching not implemented yet");
}