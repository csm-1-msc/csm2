/*
 * 4_mode_switch - 5 档按键循环逻辑模块
 * @brief 5 段循环：初始→颜色 1→颜色 2→颜色 3→流体→初始
 * 
 * 本模块提供：
 * - 5 段模式状态定义
 * - 按键循环切换逻辑
 * - 模式切换回调
 * 
 * 循环顺序：
 * 1. STYLE_INITIAL - 初始状态（默认颜色）
 * 2. STYLE_COLOR1  - 颜色 1
 * 3. STYLE_COLOR2  - 颜色 2
 * 4. STYLE_COLOR3  - 颜色 3
 * 5. STYLE_FLUID   - 流体动画
 * 然后回到 STYLE_INITIAL，循环往复
 */

#include <stdio.h>
#include "lvgl.h"
#include "esp_log.h"

static const char *TAG = "mode_switch";

// 5 段模式状态枚举
typedef enum {
    MODE_INITIAL = 0,   // 第 1 段：初始状态（默认颜色）
    MODE_COLOR1,        // 第 2 段：颜色 1
    MODE_COLOR2,        // 第 3 段：颜色 2
    MODE_COLOR3,        // 第 4 段：颜色 3
    MODE_FLUID          // 第 5 段：流体动画
} mode_state_t;

// 当前模式状态
static mode_state_t g_current_mode = MODE_INITIAL;

// 外部函数声明（由其他模块提供）
extern void create_watch_ui(lv_obj_t *scr);
extern void create_fluid_ui(lv_obj_t *scr);
extern void destroy_fluid_ui(void);
extern void init_time(void);

// 初始化模式模块
void mode_switch_init(void)
{
    g_current_mode = MODE_INITIAL;
    ESP_LOGI(TAG, "Mode switch initialized to MODE_INITIAL");
}

// 获取当前模式
mode_state_t mode_switch_get_current(void)
{
    return g_current_mode;
}

// 获取模式名称（用于调试）
const char* mode_switch_get_name(mode_state_t mode)
{
    switch (mode) {
        case MODE_INITIAL:  return "INITIAL";
        case MODE_COLOR1:   return "COLOR1";
        case MODE_COLOR2:   return "COLOR2";
        case MODE_COLOR3:   return "COLOR3";
        case MODE_FLUID:    return "FLUID";
        default:            return "UNKNOWN";
    }
}

// 处理按键按下 - 5 段循环切换
void mode_switch_on_button_press(lv_obj_t *scr)
{
    ESP_LOGI(TAG, "Button pressed: %s -> ", mode_switch_get_name(g_current_mode));
    
    switch (g_current_mode) {
        case MODE_INITIAL:
            // 初始 → 颜色 1
            g_current_mode = MODE_COLOR1;
            create_watch_ui(scr);
            ESP_LOGI(TAG, "Switched to MODE_COLOR1");
            break;
            
        case MODE_COLOR1:
            // 颜色 1 → 颜色 2
            g_current_mode = MODE_COLOR2;
            create_watch_ui(scr);
            ESP_LOGI(TAG, "Switched to MODE_COLOR2");
            break;
            
        case MODE_COLOR2:
            // 颜色 2 → 颜色 3
            g_current_mode = MODE_COLOR3;
            create_watch_ui(scr);
            ESP_LOGI(TAG, "Switched to MODE_COLOR3");
            break;
            
        case MODE_COLOR3:
            // 颜色 3 → 流体
            g_current_mode = MODE_FLUID;
            create_fluid_ui(scr);
            ESP_LOGI(TAG, "Switched to MODE_FLUID");
            break;
            
        case MODE_FLUID:
            // 流体 → 初始
            g_current_mode = MODE_INITIAL;
            destroy_fluid_ui();
            init_time();
            create_watch_ui(scr);
            ESP_LOGI(TAG, "Switched to MODE_INITIAL");
            break;
    }
}

// 切换到指定模式
void mode_switch_set_mode(mode_state_t mode, lv_obj_t *scr)
{
    if (mode == g_current_mode) {
        return;  // 已经是当前模式，无需切换
    }
    
    // 如果当前是流体模式，先销毁
    if (g_current_mode == MODE_FLUID) {
        destroy_fluid_ui();
    }
    
    g_current_mode = mode;
    
    switch (mode) {
        case MODE_INITIAL:
        case MODE_COLOR1:
        case MODE_COLOR2:
        case MODE_COLOR3:
            init_time();
            create_watch_ui(scr);
            break;
        case MODE_FLUID:
            create_fluid_ui(scr);
            break;
    }
    
    ESP_LOGI(TAG, "Switched to %s", mode_switch_get_name(mode));
}

// 判断当前是否在流体模式
bool mode_switch_is_fluid_mode(void)
{
    return g_current_mode == MODE_FLUID;
}