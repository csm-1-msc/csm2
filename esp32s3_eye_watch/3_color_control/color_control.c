/*
 * 3_color_control - 基础颜色切换模块
 * @brief 4 种颜色定义、颜色切换逻辑
 * 
 * 本模块提供：
 * - 4 种颜色定义（初始色、颜色 1、颜色 2、颜色 3）
 * - 颜色切换函数
 * - 颜色获取函数
 * 
 * 颜色定义：
 * - STYLE_INITIAL: 初始状态 - Modern Blue
 * - STYLE_COLOR1: 颜色 1 - Classic Gold
 * - STYLE_COLOR2: 颜色 2 - Minimal White
 * - STYLE_COLOR3: 颜色 3 - Digital Green
 */

#include <stdio.h>
#include "lvgl.h"
#include "esp_log.h"

static const char *TAG = "color_control";

// 颜色状态枚举
typedef enum {
    COLOR_INDEX_INITIAL = 0,  // 初始状态：默认颜色
    COLOR_INDEX_COLOR1,       // 颜色 1
    COLOR_INDEX_COLOR2,       // 颜色 2
    COLOR_INDEX_COLOR3        // 颜色 3
} color_index_t;

// 颜色数据结构（主色 + 背景色）
typedef struct {
    uint32_t primary;  // 主色
    uint32_t bg;       // 背景色
} color_pair_t;

// 4 种颜色定义
static const color_pair_t COLOR_PAIRS[4] = {
    {0x00AFFF, 0x0a0a2a},    // COLOR_INITIAL: Modern Blue
    {0xFFD700, 0x2a1a00},    // COLOR1: Classic Gold
    {0xFFFFFF, 0x0a0a0a},    // COLOR2: Minimal White/Black
    {0x00FF00, 0x001a0a}     // COLOR3: Digital Green
};

// 当前颜色索引
static color_index_t g_current_color = COLOR_INDEX_INITIAL;

// 初始化颜色模块
void color_control_init(void)
{
    g_current_color = COLOR_INDEX_INITIAL;
    ESP_LOGI(TAG, "Color control initialized");
}

// 获取当前颜色
void color_control_get_current(uint32_t *primary, uint32_t *bg)
{
    if (primary) *primary = COLOR_PAIRS[g_current_color].primary;
    if (bg) *bg = COLOR_PAIRS[g_current_color].bg;
}

// 切换到下一个颜色
void color_control_next(uint32_t *primary, uint32_t *bg)
{
    g_current_color++;
    if (g_current_color > COLOR_INDEX_COLOR3) {
        g_current_color = COLOR_INDEX_INITIAL;
    }
    
    if (primary) *primary = COLOR_PAIRS[g_current_color].primary;
    if (bg) *bg = COLOR_PAIRS[g_current_color].bg;
    
    ESP_LOGI(TAG, "Color switched to index %d", g_current_color);
}

// 设置指定颜色索引
void color_control_set_index(color_index_t index, uint32_t *primary, uint32_t *bg)
{
    if (index > COLOR_INDEX_COLOR3) {
        index = COLOR_INDEX_INITIAL;
    }
    g_current_color = index;
    
    if (primary) *primary = COLOR_PAIRS[g_current_color].primary;
    if (bg) *bg = COLOR_PAIRS[g_current_color].bg;
}

// 获取当前颜色索引
color_index_t color_control_get_index(void)
{
    return g_current_color;
}

// 获取颜色名称（用于调试）
const char* color_control_get_name(color_index_t index)
{
    switch (index) {
        case COLOR_INDEX_INITIAL: return "INITIAL";
        case COLOR_INDEX_COLOR1:  return "COLOR1";
        case COLOR_INDEX_COLOR2:  return "COLOR2";
        case COLOR_INDEX_COLOR3:  return "COLOR3";
        default:                  return "UNKNOWN";
    }
}