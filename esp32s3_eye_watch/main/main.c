/*
 * SPDX-FileCopyrightText: 2021-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

/**
 * @file
 * @brief Simple Display Example with LVGL Watch UI
 * @details ESP32-S3-EYE with LCD display and button - Step 2: Key Driver
 */

#include <stdio.h>
#include "bsp/esp-bsp.h"
#include "lvgl.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "driver/gpio.h"

// External UI functions
extern void example_lvgl_demo_ui(lv_obj_t *scr);
extern void watch_switch_style(void);

static const char *TAG = "main";

/**
 * @brief Step 2: Key Driver
 * @details Add GPIO button detection and basic key handling
 * - Button 1 (GPIO0): Switch watch style
 * - Semaphore-based button handling for reliability
 */

// Button GPIOs
#define BUTTON1_GPIO 0   // Left-bottom 1: Switch watch style

// Semaphore for button ISR
static SemaphoreHandle_t button1_sem;

// Button ISR - only signal semaphore
static void IRAM_ATTR button1_isr_handler(void *arg)
{
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    xSemaphoreGiveFromISR(button1_sem, &xHigherPriorityTaskWoken);
    if (xHigherPriorityTaskWoken) {
        portYIELD_FROM_ISR();
    }
}

// Button handler task
static void button_handler_task(void *arg)
{
    while (1) {
        // Check button1 with timeout
        if (xSemaphoreTake(button1_sem, pdMS_TO_TICKS(100)) == pdTRUE) {
            // Button 1: Switch watch style (with display lock!)
            bsp_display_lock(0);
            watch_switch_style();
            bsp_display_unlock();
        }
    }
}

void app_main(void)
{
    ESP_LOGI(TAG, "=== ESP32-S3-EYE Watch - Step 2: Key Driver ===");

    // Create semaphore for button ISR
    button1_sem = xSemaphoreCreateBinary();
    if (button1_sem == NULL) {
        ESP_LOGE(TAG, "Failed to create button1 semaphore");
        return;
    }

    // Initialize button1 GPIO (GPIO0 - Switch watch style)
    ESP_LOGI(TAG, "Initializing button1 (GPIO0)...");
    gpio_config_t io_conf = {
        .mode = GPIO_MODE_INPUT,
        .pin_bit_mask = (1ULL << BUTTON1_GPIO),
        .pull_down_en = 0,
        .pull_up_en = 1,
        .intr_type = GPIO_INTR_NEGEDGE,
    };
    gpio_config(&io_conf);
    gpio_install_isr_service(0);
    gpio_isr_handler_add(BUTTON1_GPIO, button1_isr_handler, (void *)BUTTON1_GPIO);

    // Create button handler task
    xTaskCreate(button_handler_task, "button_handler", 4096, NULL, 5, NULL);

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

    ESP_LOGI(TAG, "Step 2: Initialization complete!");
    ESP_LOGI(TAG, "Button1 (GPIO0): Switch watch style");

    // Keep running
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}