/*
 * 2_key_driver - 按键驱动模块
 * @brief 按键 GPIO 初始化、ISR 处理、信号量机制
 * 
 * 本模块提供：
 * - 按键 GPIO 初始化
 * - 中断服务程序 (ISR)
 * - 信号量用于任务通信
 * - 按键处理任务
 * 
 * 特点：单次按下触发逻辑，不区分长短按
 */

#include <stdio.h>
#include "driver/gpio.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

static const char *TAG = "key_driver";

// 按键配置
#define BUTTON1_GPIO 0   // 单一按键：GPIO0

// 信号量
static SemaphoreHandle_t button1_sem;

// 外部回调函数声明（由其他模块提供）
extern void on_button_pressed(void);

// 按键 ISR - 仅触发信号量，不执行其他操作
static void IRAM_ATTR button1_isr_handler(void *arg)
{
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    xSemaphoreGiveFromISR(button1_sem, &xHigherPriorityTaskWoken);
    if (xHigherPriorityTaskWoken) {
        portYIELD_FROM_ISR();
    }
}

// 按键处理任务
static void button_handler_task(void *arg)
{
    while (1) {
        // 等待按键信号
        if (xSemaphoreTake(button1_sem, pdMS_TO_TICKS(100)) == pdTRUE) {
            // 触发按键回调
            on_button_pressed();
        }
    }
}

// 初始化按键
void key_driver_init(void)
{
    ESP_LOGI(TAG, "Initializing key driver...");

    // 创建信号量
    button1_sem = xSemaphoreCreateBinary();
    if (button1_sem == NULL) {
        ESP_LOGE(TAG, "Failed to create button semaphore");
        return;
    }

    // 配置 GPIO
    gpio_config_t io_conf = {
        .mode = GPIO_MODE_INPUT,
        .pin_bit_mask = (1ULL << BUTTON1_GPIO),
        .pull_down_en = 0,
        .pull_up_en = 1,
        .intr_type = GPIO_INTR_NEGEDGE,
    };
    gpio_config(&io_conf);

    // 安装 ISR 服务并添加处理函数
    gpio_install_isr_service(0);
    gpio_isr_handler_add(BUTTON1_GPIO, button1_isr_handler, (void *)BUTTON1_GPIO);

    // 创建按键处理任务
    xTaskCreate(button_handler_task, "button_handler", 4096, NULL, 5, NULL);

    ESP_LOGI(TAG, "Key driver initialized (GPIO%d)", BUTTON1_GPIO);
}

// 获取按键 GPIO 号
int key_driver_get_gpio(void)
{
    return BUTTON1_GPIO;
}