/*
 * 8_sd_card_capture - SD 卡数据采集模块
 * @brief SD 卡初始化、数据记录、文件管理
 * 
 * 本模块提供：
 * - SD 卡挂载/卸载
 * - 数据采集开始/停止
 * - CSV 格式数据保存
 * - 按键控制接口
 * 
 * 使用方式：
 * - 短按：开始/停止采集
 * - 长按：退出 SD 卡模式
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <errno.h>
#include <inttypes.h>

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "freertos/queue.h"

#include "bsp/esp-bsp.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_random.h"
#include "sdmmc_cmd.h"
#include "lvgl.h"

#include "sd_card_capture.h"

static const char *TAG = "sd_card_capture";

// UI 文本长度
#define UI_TEXT_LEN 96
#define FILE_PATH_LEN 128
#define STATUS_TEXT_LEN 64

// 采样周期（毫秒）
#define SAMPLE_PERIOD_MS 500

// 状态枚举
typedef enum {
    SD_STATE_IDLE = 0,      // 空闲状态
    SD_STATE_COLLECTING,    // 采集中
    SD_STATE_ERROR          // 错误状态
} sd_capture_state_t;

// 内部状态变量
static SemaphoreHandle_t s_state_mutex = NULL;
static bool s_sd_ready = false;
static sd_capture_state_t s_state = SD_STATE_IDLE;
static FILE *s_data_file = NULL;
static char s_file_path[FILE_PATH_LEN] = {0};
static uint32_t s_sample_count = 0;
static uint32_t s_latest_value = 0;
static uint32_t s_capture_session_id = 0;
static char s_status_text[STATUS_TEXT_LEN] = {0};

// UI 组件指针
static lv_obj_t *s_title_label = NULL;
static lv_obj_t *s_state_label = NULL;
static lv_obj_t *s_value_label = NULL;
static lv_obj_t *s_count_label = NULL;
static lv_obj_t *s_sd_label = NULL;
static lv_obj_t *s_hint_label = NULL;
static lv_obj_t *s_file_label = NULL;
static lv_obj_t *s_scr = NULL;

// 数据采集任务句柄
static TaskHandle_t s_capture_task_handle = NULL;

// 退出回调函数指针
static sd_capture_exit_callback_t s_exit_callback = NULL;

// 设置状态文本（线程安全）
static void set_status_locked(const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    vsnprintf(s_status_text, sizeof(s_status_text), fmt, args);
    va_end(args);
}

// 刷新 UI（线程安全）
static void refresh_ui(void)
{
    bool collecting = false;
    bool sd_ready = false;
    uint32_t latest_value = 0;
    uint32_t sample_count = 0;
    char status_text[UI_TEXT_LEN];
    char file_path[FILE_PATH_LEN];

    if (xSemaphoreTake(s_state_mutex, pdMS_TO_TICKS(50)) != pdTRUE) {
        return;
    }

    collecting = (s_state == SD_STATE_COLLECTING);
    sd_ready = s_sd_ready;
    latest_value = s_latest_value;
    sample_count = s_sample_count;
    snprintf(status_text, sizeof(status_text), "%s", s_status_text);
    snprintf(file_path, sizeof(file_path), "%s", s_file_path);

    xSemaphoreGive(s_state_mutex);

    if (!bsp_display_lock(50)) {
        return;
    }

    if (s_state_label) {
        lv_label_set_text_fmt(s_state_label, "State: %s", collecting ? "COLLECTING" : "IDLE");
    }
    if (s_value_label) {
        lv_label_set_text_fmt(s_value_label, "Data: %" PRIu32, latest_value);
    }
    if (s_count_label) {
        lv_label_set_text_fmt(s_count_label, "Samples: %" PRIu32, sample_count);
    }
    if (s_sd_label) {
        lv_label_set_text_fmt(s_sd_label, "SD: %s", sd_ready ? "READY" : "NOT READY");
    }
    if (s_hint_label) {
        lv_label_set_text_fmt(s_hint_label, "Short: Start/Stop  Long: Exit");
    }
    if (s_file_label) {
        lv_label_set_text_fmt(s_file_label, "Status: %s\nFile: %s",
                              status_text,
                              file_path[0] ? file_path : "(none)");
    }

    bsp_display_unlock();
}

// 停止采集（线程安全）
static void stop_collection_locked(const char *reason)
{
    if (s_data_file) {
        fclose(s_data_file);
        s_data_file = NULL;
        ESP_LOGI(TAG, "File closed: %s", s_file_path);
    }
    s_state = SD_STATE_IDLE;
    set_status_locked("%s", reason);
    s_file_path[0] = '\0';
}

// 开始采集（线程安全）
static esp_err_t start_collection_locked(void)
{
    if (!s_sd_ready) {
        set_status_locked("SD not ready");
        return ESP_ERR_INVALID_STATE;
    }

    if (s_state == SD_STATE_COLLECTING) {
        return ESP_OK;
    }

    // 生成文件名
    uint32_t file_tag = (uint32_t)((esp_timer_get_time() / 1000) % 100000);
    s_capture_session_id++;
    if (s_capture_session_id < 100000) {
        file_tag = s_capture_session_id;
    }
    snprintf(s_file_path, sizeof(s_file_path), BSP_SD_MOUNT_POINT "/CAP%05" PRIu32 ".CSV", file_tag);

    // 打开文件
    s_data_file = fopen(s_file_path, "w");
    if (!s_data_file) {
        int err = errno;
        s_file_path[0] = '\0';
        set_status_locked("Open failed (errno=%d)", err);
        ESP_LOGE(TAG, "Failed to open file: %s", s_file_path);
        return ESP_FAIL;
    }

    // 写入 CSV 表头
    fprintf(s_data_file, "index,timestamp_ms,value\n");
    fflush(s_data_file);

    // 重置计数器
    s_latest_value = 0;
    s_sample_count = 0;
    s_state = SD_STATE_COLLECTING;
    set_status_locked("Collecting");

    ESP_LOGI(TAG, "Capture started: %s", s_file_path);

    return ESP_OK;
}

// 数据采集任务
static void capture_task(void *arg)
{
    (void)arg;

    while (true) {
        bool should_collect = false;

        if (xSemaphoreTake(s_state_mutex, pdMS_TO_TICKS(200)) == pdTRUE) {
            should_collect = (s_state == SD_STATE_COLLECTING && s_data_file != NULL);
            xSemaphoreGive(s_state_mutex);
        }

        if (should_collect) {
            // 模拟采集数据（实际项目中可替换为传感器数据）
            uint32_t value = esp_random() % 10000;
            int64_t timestamp_ms = esp_timer_get_time() / 1000;

            if (fprintf(s_data_file, "%" PRIu32 ",%" PRId64 ",%" PRIu32 "\n",
                        s_sample_count + 1, timestamp_ms, value) < 0) {
                ESP_LOGE(TAG, "Write failed");
                if (xSemaphoreTake(s_state_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
                    stop_collection_locked("Write error");
                    xSemaphoreGive(s_state_mutex);
                }
            } else {
                fflush(s_data_file);
                if (xSemaphoreTake(s_state_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
                    s_latest_value = value;
                    s_sample_count++;
                    xSemaphoreGive(s_state_mutex);
                }
            }
        }

        refresh_ui();
        vTaskDelay(pdMS_TO_TICKS(SAMPLE_PERIOD_MS));
    }
}

// SD 卡初始化
static esp_err_t init_sdcard(void)
{
    esp_err_t ret = bsp_sdcard_mount();

    if (xSemaphoreTake(s_state_mutex, pdMS_TO_TICKS(200)) != pdTRUE) {
        return ESP_FAIL;
    }

    if (ret == ESP_OK) {
        s_sd_ready = true;
        set_status_locked("SD ready");
        ESP_LOGI(TAG, "SD card mounted at %s", BSP_SD_MOUNT_POINT);

        sdmmc_card_t *card = bsp_sdcard_get_handle();
        if (card) {
            ESP_LOGI(TAG, "SD card info:");
            sdmmc_card_print_info(stdout, card);
        }
    } else {
        s_sd_ready = false;
        set_status_locked("SD mount failed");
        ESP_LOGE(TAG, "Failed to mount SD card: %s", esp_err_to_name(ret));
    }

    xSemaphoreGive(s_state_mutex);

    return ret;
}

// 创建 UI
static void create_ui(lv_obj_t *scr)
{
    s_scr = scr;

    if (!bsp_display_lock(0)) {
        return;
    }

    // 清理屏幕并设置背景
    lv_obj_clean(scr);
    lv_obj_set_style_bg_color(scr, lv_color_hex(0x101820), 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
    lv_obj_set_style_text_color(scr, lv_color_hex(0xE6E6E6), 0);

    // 标题
    s_title_label = lv_label_create(scr);
    lv_label_set_text(s_title_label, "SD Card Capture");
    lv_obj_set_style_text_color(s_title_label, lv_color_hex(0x6EE7FF), 0);
    lv_obj_align(s_title_label, LV_ALIGN_TOP_LEFT, 10, 10);

    // 提示
    s_hint_label = lv_label_create(scr);
    lv_obj_set_width(s_hint_label, 220);
    lv_label_set_long_mode(s_hint_label, LV_LABEL_LONG_WRAP);
    lv_label_set_text(s_hint_label, "Short press: Start/Stop\nLong press: Exit");
    lv_obj_align(s_hint_label, LV_ALIGN_TOP_LEFT, 10, 35);

    // 状态
    s_state_label = lv_label_create(scr);
    lv_obj_align(s_state_label, LV_ALIGN_TOP_LEFT, 10, 75);

    // 数据值
    s_value_label = lv_label_create(scr);
    lv_obj_align(s_value_label, LV_ALIGN_TOP_LEFT, 10, 100);

    // 采样计数
    s_count_label = lv_label_create(scr);
    lv_obj_align(s_count_label, LV_ALIGN_TOP_LEFT, 10, 125);

    // SD 状态
    s_sd_label = lv_label_create(scr);
    lv_obj_align(s_sd_label, LV_ALIGN_TOP_LEFT, 10, 150);

    // 文件信息
    s_file_label = lv_label_create(scr);
    lv_obj_set_width(s_file_label, 220);
    lv_label_set_long_mode(s_file_label, LV_LABEL_LONG_WRAP);
    lv_obj_align(s_file_label, LV_ALIGN_TOP_LEFT, 10, 175);

    bsp_display_unlock();

    ESP_LOGI(TAG, "UI created");
}

// 销毁 UI
static void destroy_ui(void)
{
    s_title_label = NULL;
    s_state_label = NULL;
    s_value_label = NULL;
    s_count_label = NULL;
    s_sd_label = NULL;
    s_hint_label = NULL;
    s_file_label = NULL;
    s_scr = NULL;

    ESP_LOGI(TAG, "UI destroyed");
}

// ============================================================================
// 公共 API 实现
// ============================================================================

esp_err_t sd_capture_init(sd_capture_exit_callback_t on_exit)
{
    ESP_LOGI(TAG, "Initializing SD card capture module");

    // 创建互斥锁
    s_state_mutex = xSemaphoreCreateMutex();
    if (!s_state_mutex) {
        ESP_LOGE(TAG, "Failed to create mutex");
        return ESP_FAIL;
    }

    // 设置退出回调
    s_exit_callback = on_exit;

    // 初始化状态
    s_state = SD_STATE_IDLE;
    s_sd_ready = false;
    s_sample_count = 0;
    s_capture_session_id = 0;
    s_file_path[0] = '\0';
    set_status_locked("Initializing");

    // 创建 UI
    lv_obj_t *scr = lv_disp_get_scr_act(NULL);
    if (scr) {
        create_ui(scr);
    }

    // 初始化 SD 卡
    esp_err_t ret = init_sdcard();

    // 刷新 UI
    refresh_ui();

    // 启动采集任务
    xTaskCreate(capture_task, "sd_capture", 4096, NULL, 5, &s_capture_task_handle);

    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "SD card capture initialized successfully");
    } else {
        ESP_LOGW(TAG, "SD card not available, will retry in background");
    }

    return ret;
}

void sd_capture_deinit(void)
{
    ESP_LOGI(TAG, "Deinitializing SD card capture module");

    // 停止采集
    if (xSemaphoreTake(s_state_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        if (s_state == SD_STATE_COLLECTING) {
            stop_collection_locked("Deinitializing");
        }
        xSemaphoreGive(s_state_mutex);
    }

    // 删除任务
    if (s_capture_task_handle) {
        vTaskDelete(s_capture_task_handle);
        s_capture_task_handle = NULL;
    }

    // 销毁 UI
    destroy_ui();

    // 删除互斥锁
    if (s_state_mutex) {
        vSemaphoreDelete(s_state_mutex);
        s_state_mutex = NULL;
    }

    ESP_LOGI(TAG, "SD card capture deinitialized");
}

esp_err_t sd_capture_start(void)
{
    ESP_LOGI(TAG, "Starting data capture");

    if (xSemaphoreTake(s_state_mutex, pdMS_TO_TICKS(100)) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }

    esp_err_t ret = start_collection_locked();

    xSemaphoreGive(s_state_mutex);

    refresh_ui();

    return ret;
}

void sd_capture_stop(void)
{
    ESP_LOGI(TAG, "Stopping data capture");

    if (xSemaphoreTake(s_state_mutex, pdMS_TO_TICKS(100)) != pdTRUE) {
        return;
    }

    if (s_state == SD_STATE_COLLECTING) {
        char reason[STATUS_TEXT_LEN];
        snprintf(reason, sizeof(reason), "Saved %" PRIu32 " samples", s_sample_count);
        stop_collection_locked(reason);
        ESP_LOGI(TAG, "Capture stopped: %" PRIu32 " samples saved", s_sample_count);
    }

    xSemaphoreGive(s_state_mutex);

    refresh_ui();
}

void sd_capture_toggle(void)
{
    ESP_LOGI(TAG, "Toggling data capture");

    if (xSemaphoreTake(s_state_mutex, pdMS_TO_TICKS(100)) != pdTRUE) {
        return;
    }

    if (s_state == SD_STATE_COLLECTING) {
        char reason[STATUS_TEXT_LEN];
        snprintf(reason, sizeof(reason), "Saved %" PRIu32 " samples", s_sample_count);
        stop_collection_locked(reason);
        ESP_LOGI(TAG, "Capture stopped: %" PRIu32 " samples saved", s_sample_count);
    } else {
        esp_err_t ret = start_collection_locked();
        if (ret == ESP_OK) {
            ESP_LOGI(TAG, "Capture started: %s", s_file_path);
        } else {
            ESP_LOGW(TAG, "Failed to start capture");
        }
    }

    xSemaphoreGive(s_state_mutex);

    refresh_ui();
}

void sd_capture_handle_long_press(void)
{
    ESP_LOGI(TAG, "Long press detected, exiting SD capture mode");

    // 停止采集
    if (xSemaphoreTake(s_state_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        if (s_state == SD_STATE_COLLECTING) {
            char reason[STATUS_TEXT_LEN];
            snprintf(reason, sizeof(reason), "Saved %" PRIu32 " samples", s_sample_count);
            stop_collection_locked(reason);
        }
        xSemaphoreGive(s_state_mutex);
    }

    // 调用退出回调
    if (s_exit_callback) {
        s_exit_callback();
    }
}

bool sd_capture_is_collecting(void)
{
    bool collecting = false;
    if (xSemaphoreTake(s_state_mutex, pdMS_TO_TICKS(50)) == pdTRUE) {
        collecting = (s_state == SD_STATE_COLLECTING);
        xSemaphoreGive(s_state_mutex);
    }
    return collecting;
}

bool sd_capture_is_sd_ready(void)
{
    return s_sd_ready;
}

uint32_t sd_capture_get_sample_count(void)
{
    uint32_t count = 0;
    if (xSemaphoreTake(s_state_mutex, pdMS_TO_TICKS(50)) == pdTRUE) {
        count = s_sample_count;
        xSemaphoreGive(s_state_mutex);
    }
    return count;
}

const char* sd_capture_get_current_file(void)
{
    return s_file_path[0] ? s_file_path : NULL;
}

void sd_capture_exit(void)
{
    ESP_LOGI(TAG, "Exiting SD capture mode");

    // 停止采集
    if (xSemaphoreTake(s_state_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        if (s_state == SD_STATE_COLLECTING) {
            char reason[STATUS_TEXT_LEN];
            snprintf(reason, sizeof(reason), "Saved %" PRIu32 " samples", s_sample_count);
            stop_collection_locked(reason);
            ESP_LOGI(TAG, "Capture stopped: %" PRIu32 " samples saved", s_sample_count);
        }
        xSemaphoreGive(s_state_mutex);
    }

    // 删除任务
    if (s_capture_task_handle) {
        vTaskDelete(s_capture_task_handle);
        s_capture_task_handle = NULL;
    }

    // 销毁 UI
    destroy_ui();

    // 调用退出回调返回手表模式
    if (s_exit_callback) {
        ESP_LOGI(TAG, "Calling exit callback to return to watch mode");
        s_exit_callback();
    }

    // 反初始化模块（清理资源）
    if (s_state_mutex) {
        vSemaphoreDelete(s_state_mutex);
        s_state_mutex = NULL;
    }

    ESP_LOGI(TAG, "SD capture mode exited");
}
