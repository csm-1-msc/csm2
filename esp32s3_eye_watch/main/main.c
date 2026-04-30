/*
 * SPDX-FileCopyrightText: 2021-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

/**
 * @file
 * @brief Simple Display Example with LVGL Watch UI
 * @details ESP32-S3-EYE with LCD display - Step 1: Project Base
 */

#include <stdio.h>
#include "bsp/esp-bsp.h"
#include "lvgl.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

// External UI function
extern void example_lvgl_demo_ui(lv_obj_t *scr);

static const char *TAG = "main";

/**
 * @brief Step 1: Project Base
 * @details Basic display initialization with static watch UI
 * - No button handling
 * - No color switching
 * - No animation
 */

void app_main(void)
{
    ESP_LOGI(TAG, "=== ESP32-S3-EYE Watch - Step 1: Project Base ===");

    // Start display
    ESP_LOGI(TAG, "Starting display...");
    bsp_display_start();

    ESP_LOGI(TAG, "Display LVGL watch UI");
    bsp_display_lock(0);

    lv_obj_t *scr = lv_disp_get_scr_act(NULL);
    example_lvgl_demo_ui(scr);

    bsp_display_unlock();

    // Set backlight
    ESP_LOGI(TAG, "Setting backlight...");
    bsp_display_brightness_set(80);
    bsp_display_backlight_on();

    ESP_LOGI(TAG, "Step 1: Initialization complete!");
    ESP_LOGI(TAG, "Basic watch UI displayed with static time");

    // Keep running
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}