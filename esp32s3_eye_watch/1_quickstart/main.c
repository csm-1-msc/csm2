/*
 * 1_quickstart - 基础工程框架
 * @brief 基础初始化、显示启动、主入口
 * 
 * 本模块提供：
 * - 应用入口 app_main
 * - 显示初始化
 * - 基础框架搭建
 */

#include <stdio.h>
#include "bsp/esp-bsp.h"
#include "lvgl.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "quickstart";

// 外部函数声明（由其他模块提供）
extern void example_lvgl_demo_ui(lv_obj_t *scr);

void app_main(void)
{
    ESP_LOGI(TAG, "=== 1_quickstart - Base Framework ===");

    // 启动显示
    ESP_LOGI(TAG, "Starting display...");
    lv_display_t *disp = bsp_display_start();
    ESP_LOGI(TAG, "bsp_display_start() returned: %p", disp);

    // 等待显示初始化
    vTaskDelay(pdMS_TO_TICKS(100));

    ESP_LOGI(TAG, "Display initialized");
    
    // 设置背光
    ESP_LOGI(TAG, "Setting backlight...");
    bsp_display_brightness_set(80);
    bsp_display_backlight_on();
    ESP_LOGI(TAG, "Backlight set to 80%%");

    ESP_LOGI(TAG, "1_quickstart initialization complete!");

    // 保持运行
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}