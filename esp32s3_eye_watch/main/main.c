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
#include <string.h>
#include "bsp/esp-bsp.h"
#include "lvgl.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "driver/gpio.h"
#include "esp_wifi.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_sntp.h"
#include <time.h>

// External UI functions
extern void example_lvgl_demo_ui(lv_obj_t *scr);
extern void watch_switch_style(void);
extern void watch_switch_ui(void);
extern void watch_update_time_from_ntp(time_t ts);

static const char *TAG = "main";

// Button GPIOs
#define BUTTON1_GPIO 0   // Single button: Switch watch style (5-cycle) and toggle watch/fluid

// Semaphore for button ISR
static SemaphoreHandle_t button1_sem;

// WiFi and NTP configuration
#define WIFI_SSID "432"
#define WIFI_PASSWORD "88888888"
#define NTP_SERVER "ntp.aliyun.com"
#define NTP_TIMEZONE "CST-8"  // China Standard Time (UTC+8)

static bool g_wifi_initialized = false;
static bool g_ntp_synchronized = false;

// Button ISR - only signal semaphore, do nothing else
static void IRAM_ATTR button1_isr_handler(void *arg)
{
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    xSemaphoreGiveFromISR(button1_sem, &xHigherPriorityTaskWoken);
    if (xHigherPriorityTaskWoken) {
        portYIELD_FROM_ISR();
    }
}

// WiFi event handler
static void wifi_event_handler(void *arg, esp_event_base_t event_base,
                               int32_t event_id, void *event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        esp_wifi_connect();
        ESP_LOGW(TAG, "WiFi disconnected, retrying...");
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ESP_LOGI(TAG, "WiFi connected! Got IP");
        g_wifi_initialized = true;
        
        // Initialize SNTP for NTP time sync
        ESP_LOGI(TAG, "Initializing SNTP...");
        esp_sntp_config_t config = ESP_SNTP_DEFAULT_CONFIG(NTP_SERVER);
        config.start = true;
        esp_sntp_init(&config);
        
        // Set timezone
        setenv("TZ", NTP_TIMEZONE, 1);
        tzset();
    }
}

// SNTP event handler for time sync
static void sntp_event_handler(void *arg, esp_event_base_t event_base,
                               int32_t event_id, void *event_data)
{
    if (event_base == SNTP_EVENT && event_id == SNTP_EVENT_TIME_SYNC) {
        struct timeval tv;
        esp_sntp_get_startup_time(&tv);
        struct tm *timeinfo = localtime((const time_t *)&tv.tv_sec);
        
        char time_str[64];
        strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", timeinfo);
        ESP_LOGI(TAG, "NTP time synchronized: %s", time_str);
        
        g_ntp_synchronized = true;
        
        // Update UI with synchronized time
        watch_update_time_from_ntp(tv.tv_sec);
    }
}

// Button handler task - single button controls everything
static void button_handler_task(void *arg)
{
    while (1) {
        // Check button1 with timeout
        if (xSemaphoreTake(button1_sem, pdMS_TO_TICKS(100)) == pdTRUE) {
            // Button 1: Switch watch style (5-cycle) and toggle watch/fluid (with display lock!)
            bsp_display_lock(0);
            watch_switch_style();  // This handles both 5-cycle and watch/fluid toggle
            bsp_display_unlock();
        }
    }
}

// WiFi initialization task
static void wifi_init_task(void *arg)
{
    ESP_LOGI(TAG, "Initializing WiFi...");
    
    // Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_LOGW(TAG, "NVS partition was truncated, erasing...");
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    
    // Initialize TCP/IP stack
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    
    // Create WiFi interface
    esp_netif_create_default_wifi_sta();
    
    // Initialize WiFi
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    
    // Register event handlers
    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL, NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL, NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        SNTP_EVENT, SNTP_EVENT_TIME_SYNC, &sntp_event_handler, NULL, NULL));
    
    // Set WiFi mode and connect
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    wifi_config_t wifi_config = {0};
    strncpy((char *)wifi_config.sta.ssid, WIFI_SSID, sizeof(wifi_config.sta.ssid) - 1);
    strncpy((char *)wifi_config.sta.password, WIFI_PASSWORD, sizeof(wifi_config.sta.password) - 1);
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_ERROR_CHECK(esp_wifi_connect());
    
    ESP_LOGI(TAG, "WiFi started, connecting to %s...", WIFI_SSID);
    
    // Wait for connection
    int timeout = 0;
    while (!g_wifi_initialized && timeout < 30) {
        vTaskDelay(pdMS_TO_TICKS(1000));
        timeout++;
        ESP_LOGI(TAG, "Connecting to WiFi... (%d/30)", timeout);
    }
    
    if (g_wifi_initialized) {
        ESP_LOGI(TAG, "WiFi connected successfully!");
    } else {
        ESP_LOGW(TAG, "WiFi connection timeout, continuing without WiFi");
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

    // Set backlight
    ESP_LOGI(TAG, "Setting backlight...");
    bsp_display_brightness_set(80);
    bsp_display_backlight_on();
    ESP_LOGI(TAG, "Backlight set to 80%% and turned ON");

    ESP_LOGI(TAG, "Initialization complete!");
    ESP_LOGI(TAG, "Button1 (GPIO0): Press to cycle through 5 styles (COLOR1->COLOR2->COLOR3->FLUID->STYLE1->...)");
    ESP_LOGI(TAG, "WiFi: Connecting to %s...", WIFI_SSID);
    ESP_LOGI(TAG, "NTP: Will sync time from %s (UTC+8)", NTP_SERVER);

    // Start WiFi initialization in a separate task
    xTaskCreate(wifi_init_task, "wifi_init", 8192, NULL, 4, NULL);

    // Keep running
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}