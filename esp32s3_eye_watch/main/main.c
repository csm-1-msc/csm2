/*
 * SPDX-FileCopyrightText: 2021-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

/**
 * @file
 * @brief Simple Display Example with LVGL Watch UI
 * @details ESP32-S3-EYE with LCD display and button to switch watch styles
 */

#include <stdio.h>
#include "bsp/esp-bsp.h"
#include "lvgl.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "driver/gpio.h"

// External UI functions
extern void example_lvgl_demo_ui(lv_obj_t *scr);
extern void watch_switch_style(void);
extern void watch_switch_ui(void);
extern bool watch_is_fluid_mode(void);
extern void watch_return_to_watch(void);  // New function to return from fluid to watch

#include "gravity_control.h"
#include "wifi_connect.h"

static const char *TAG = "main";

// Button GPIOs
#define BUTTON1_GPIO 0   // Single button: Switch watch style (5-cycle) and toggle watch/fluid

// Semaphore for button ISR
static SemaphoreHandle_t button1_sem;

// Button ISR - only signal semaphore, do nothing else
static void IRAM_ATTR button1_isr_handler(void *arg)
{
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    xSemaphoreGiveFromISR(button1_sem, &xHigherPriorityTaskWoken);
    if (xHigherPriorityTaskWoken) {
        portYIELD_FROM_ISR();
    }
}

// Gravity auto-detect task - periodically read accelerometer and update gravity direction
// Reference: display_rotation's ACCEL_ROTATION_HOLD_MS debouncing mechanism
static void gravity_detect_task(void *arg)
{
    ESP_LOGI(TAG, "Gravity auto-detect task started (reading QMA6100P accelerometer)");
    
    while (1) {
        // Update gravity direction every 200ms (reference: display_rotation)
        gravity_control_update_from_accelerometer();
        vTaskDelay(pdMS_TO_TICKS(200));
    }
}

// Button handler task - single button controls everything
// Supports short press and long press detection
// Short press: Switch style (watch mode) or switch gravity (fluid mode)
// Long press (1s): Return to watch mode (fluid mode only)
static void button_handler_task(void *arg)
{
    static bool button_was_pressed = false;
    static int64_t button_press_start_time_ms = 0;
    const int64_t LONG_PRESS_MS = 1000;  // 1 second long press
    const int64_t DEBOUNCE_MS = 150;     // 150ms debounce
    
    while (1) {
        // Wait for button press event (ISR signals on falling edge)
        if (xSemaphoreTake(button1_sem, pdMS_TO_TICKS(100)) == pdTRUE) {
            int64_t now_ms = esp_timer_get_time() / 1000;  // Convert to milliseconds
            
            // Check if button is actually pressed (active low)
            bool button_pressed = gpio_get_level(BUTTON1_GPIO) == 0;
            
            if (button_pressed && !button_was_pressed) {
                // Button just pressed - record start time
                button_press_start_time_ms = now_ms;
                button_was_pressed = true;
                ESP_LOGD(TAG, "Button pressed at %lld ms", now_ms);
            }
        }
        
        // Check for button release by polling
        if (button_was_pressed) {
            bool button_pressed = gpio_get_level(BUTTON1_GPIO) == 0;
            
            if (!button_pressed) {
                // Button just released - calculate press duration
                int64_t now_ms = esp_timer_get_time() / 1000;
                int64_t press_duration = now_ms - button_press_start_time_ms;
                
                ESP_LOGD(TAG, "Button released after %lld ms", press_duration);
                
                if (press_duration > DEBOUNCE_MS) {
                    // Valid press - check if short or long
                    if (press_duration >= LONG_PRESS_MS) {
                        // Long press
                        ESP_LOGI(TAG, "Long press detected (%lld ms)", press_duration);
                        if (watch_is_fluid_mode()) {
                            // Long press in fluid mode: return to watch mode
                            bsp_display_lock(0);
                            ESP_LOGI(TAG, "Fluid mode: long press, returning to watch mode");
                            watch_return_to_watch();
                            bsp_display_unlock();
                        }
                        // Long press in watch mode: do nothing (only short press switches)
                    } else {
                        // Short press
                        ESP_LOGI(TAG, "Short press detected (%lld ms)", press_duration);
                        bsp_display_lock(0);
                        
                        if (watch_is_fluid_mode()) {
                            // Short press in fluid mode: switch gravity direction
                            ESP_LOGI(TAG, "Fluid mode: short press, switching gravity direction");
                            watch_switch_style();
                        } else {
                            // Short press in watch mode: switch watch style
                            ESP_LOGI(TAG, "Watch mode: short press, switching style");
                            watch_switch_style();
                        }
                        
                        bsp_display_unlock();
                    }
                }
                
                // Reset state
                button_was_pressed = false;
                button_press_start_time_ms = 0;
            }
        }
        
        vTaskDelay(pdMS_TO_TICKS(20));  // Poll every 20ms
    }
}

// WiFi and NTP initialization task
static void wifi_ntp_init_task(void *arg)
{
    ESP_LOGI(TAG, "===== WiFi/NTP init task started =====");
    
    // Initialize and connect to WiFi
    ESP_LOGI(TAG, "Initializing WiFi...");
    esp_err_t wifi_init_ret = wifi_connect_init();
    if (wifi_init_ret != ESP_OK) {
        ESP_LOGE(TAG, "WiFi init failed: %s", esp_err_to_name(wifi_init_ret));
    } else {
        ESP_LOGI(TAG, "WiFi init completed");
    }
    
    // Connect to WiFi network (SSID: 431, Password: 88888888)
    ESP_LOGI(TAG, "Connecting to WiFi: 431");
    if (wifi_connect("431", "88888888") == ESP_OK) {
        ESP_LOGI(TAG, "WiFi connected successfully");
        
        // Get IP address
        char ip_buf[32];
        ESP_LOGI(TAG, "IP Address: %s", wifi_get_ip(ip_buf, sizeof(ip_buf)));
        
        // Initialize NTP time sync with China timezone (UTC+8)
        ESP_LOGI(TAG, "Initializing NTP time sync (CST-8)...");
        ntp_init("CST-8");
        
        // Wait for NTP sync with longer timeout (30 seconds)
        ESP_LOGI(TAG, "Waiting for NTP sync (30s timeout)...");
        if (ntp_wait_for_sync(30000)) {
            ESP_LOGI(TAG, "NTP time synchronized successfully");
            
            // Get and display current time
            struct tm timeinfo;
            if (ntp_get_time(&timeinfo)) {
                char timebuf[64];
                strftime(timebuf, sizeof(timebuf), "%Y-%m-%d %H:%M:%S", &timeinfo);
                ESP_LOGI(TAG, "Current time: %s", timebuf);
            }
        } else {
            ESP_LOGW(TAG, "NTP sync timeout, will retry in background");
        }
    } else {
        ESP_LOGW(TAG, "WiFi connection failed, will use local time");
    }
    
    vTaskDelete(NULL);
}

void app_main(void)
{
    ESP_LOGI(TAG, "=== ESP32-S3-EYE Watch with Fluid Animation ===");

    // Create semaphore for button ISR
    button1_sem = xSemaphoreCreateBinary();
    if (button1_sem == NULL) {
        ESP_LOGE(TAG, "Failed to create button1 semaphore");
        return;
    }

    // Initialize button1 GPIO (GPIO0 - Single button for all controls)
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
    lv_display_t *disp = bsp_display_start();
    ESP_LOGI(TAG, "bsp_display_start() returned: %p", disp);

    // Wait a bit for display initialization
    vTaskDelay(pdMS_TO_TICKS(100));

    ESP_LOGI(TAG, "Display LVGL watch UI");
    
    // Lock with timeout to ensure we can proceed even if there's an issue
    if (bsp_display_lock(1000)) {
        ESP_LOGI(TAG, "Display lock acquired");
        
        lv_obj_t *scr = lv_disp_get_scr_act(NULL);
        ESP_LOGI(TAG, "Active screen: %p", scr);
        
        if (scr) {
            example_lvgl_demo_ui(scr);
            ESP_LOGI(TAG, "UI created successfully");
        } else {
            ESP_LOGE(TAG, "Failed to get active screen!");
        }
        
        bsp_display_unlock();
    } else {
        ESP_LOGE(TAG, "Failed to acquire display lock!");
    }

    // Initialize gravity control module
    ESP_LOGI(TAG, "Initializing gravity control...");
    gravity_control_init();
    
    // Enable gravity auto-detect for fluid mode
    // Reference: display_rotation's debouncing mechanism
    ESP_LOGI(TAG, "Enabling gravity auto-detect (QMA6100P)...");
    gravity_control_enable_auto_detect(true);
    
    // Create gravity auto-detect task
    xTaskCreate(gravity_detect_task, "gravity_detect", 4096, NULL, 4, NULL);
    ESP_LOGI(TAG, "Gravity auto-detect task created");
    
    // Set backlight
    ESP_LOGI(TAG, "Setting backlight...");
    bsp_display_brightness_set(80);
    bsp_display_backlight_on();
    ESP_LOGI(TAG, "Backlight set to 80%% and turned ON");

    ESP_LOGI(TAG, "Initialization complete!");
    ESP_LOGI(TAG, "Button1 (GPIO0) controls:");
    ESP_LOGI(TAG, "  - Short press (<1s): Switch style (watch mode) or switch gravity (fluid mode)");
    ESP_LOGI(TAG, "  - Long press (>=1s): Return to watch mode (fluid mode only)");
    ESP_LOGI(TAG, "Gravity auto-detect is enabled (QMA6100P accelerometer, reference: display_rotation)");
    
    // Start WiFi/NTP initialization in background task
    // HIGHEST priority (10) to ensure it runs first
    ESP_LOGI(TAG, "Starting WiFi/NTP initialization with HIGHEST priority...");
    BaseType_t task_created = xTaskCreate(wifi_ntp_init_task, "wifi_ntp_init", 16384, NULL, 10, NULL);
    if (task_created != pdPASS) {
        ESP_LOGE(TAG, "Failed to create WiFi/NTP task! Return code: %d", task_created);
        ESP_LOGE(TAG, "Free heap: %lu bytes", (unsigned long)esp_get_free_heap_size());
    } else {
        ESP_LOGI(TAG, "WiFi/NTP task created successfully with priority 10");
    }
    
    // Give WiFi task time to start and initialize
    vTaskDelay(pdMS_TO_TICKS(500));

    // Keep running - delete this task to let other tasks run
    ESP_LOGI(TAG, "Main task completed, deleting itself");
    vTaskDelete(NULL);
}