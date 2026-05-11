/*
 * 7_display_ui - 显示/UI 界面模块
 * @brief Watch UI 创建、时间显示、日期显示、UI 组件管理
 * 
 * 本模块提供：
 * - Watch UI 创建（主界面）
 * - 时间/日期/星期显示
 * - UI 定时器管理
 * - 颜色应用
 * 
 * 依赖：
 * - 3_color_control: 获取颜色配置
 */

#include <stdio.h>
#include <time.h>
#include "lvgl.h"
#include "esp_log.h"
#include "esp_timer.h"

static const char *TAG = "display_ui";

// UI 组件指针
static lv_obj_t *g_time_label = NULL;
static lv_obj_t *g_date_label = NULL;
static lv_obj_t *g_weekday_label = NULL;
static lv_timer_t *g_watch_timer = NULL;

// 时间状态
static time_t g_current_ts = 0;
static bool g_in_fluid_mode = false;

// 外部依赖声明
extern void color_control_get_current(uint32_t *primary, uint32_t *bg);

// 初始化时间
static void init_time(void)
{
    struct tm timeinfo = {0};
    timeinfo.tm_year = 126;  // 2026 - 1900
    timeinfo.tm_mon = 3;     // April (0-11)
    timeinfo.tm_mday = 8;
    timeinfo.tm_hour = 12;
    timeinfo.tm_min = 0;
    timeinfo.tm_sec = 0;
    g_current_ts = mktime(&timeinfo);
}

// 更新标签显示
static void update_labels(void)
{
    struct tm timeinfo;
    localtime_r(&g_current_ts, &timeinfo);

    char buf[32];

    // 时间：HH:MM:SS（24 小时制）
    snprintf(buf, sizeof(buf), "%02d:%02d:%02d",
             timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
    if (g_time_label) lv_label_set_text(g_time_label, buf);

    // 日期：YYYY MM DD
    snprintf(buf, sizeof(buf), "2026 04 08");
    if (g_date_label) lv_label_set_text(g_date_label, buf);

    // 星期：固定为 "3"（星期三）
    snprintf(buf, sizeof(buf), "3");
    if (g_weekday_label) lv_label_set_text(g_weekday_label, buf);
}

// 定时器回调
static void timer_cb(lv_timer_t *timer)
{
    (void)timer;
    if (!g_in_fluid_mode && g_watch_timer == timer && 
        g_time_label && g_date_label && g_weekday_label) {
        g_current_ts++;
        update_labels();
    }
}

// 创建 Watch UI
void create_watch_ui(lv_obj_t *scr)
{
    ESP_LOGI(TAG, "Creating watch UI");
    
    g_in_fluid_mode = false;
    
    // 清理屏幕
    lv_obj_clean(scr);
    lv_obj_set_style_bg_color(scr, lv_color_hex(0x000000), 0);

    // 获取当前颜色
    uint32_t color, bg_color;
    color_control_get_current(&color, &bg_color);

    // 创建主框架
    lv_obj_t *main_frame = lv_obj_create(scr);
    lv_obj_set_size(main_frame, 220, 220);
    lv_obj_center(main_frame);
    lv_obj_set_style_bg_color(main_frame, lv_color_hex(bg_color), 0);
    lv_obj_set_style_border_width(main_frame, 3, 0);
    lv_obj_set_style_border_color(main_frame, lv_color_hex(color), 0);
    lv_obj_set_style_radius(main_frame, 15, 0);
    lv_obj_set_style_pad_all(main_frame, 0, 0);

    // 顶部标题
    lv_obj_t *title = lv_label_create(main_frame);
    lv_label_set_text(title, "ESP32-S3-EYE");
    lv_obj_set_style_text_color(title, lv_color_hex(color), 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_14, 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 15);

    // 中心时间
    g_time_label = lv_label_create(main_frame);
    lv_label_set_text(g_time_label, "12:00:00");
    lv_obj_set_style_text_color(g_time_label, lv_color_hex(color), 0);
    lv_obj_set_style_text_font(g_time_label, &lv_font_montserrat_14, 0);
    lv_obj_align(g_time_label, LV_ALIGN_CENTER, 0, -10);

    // 日期
    g_date_label = lv_label_create(main_frame);
    lv_label_set_text(g_date_label, "2026 04 08");
    lv_obj_set_style_text_color(g_date_label, lv_color_hex(color), 0);
    lv_obj_set_style_text_font(g_date_label, &lv_font_montserrat_14, 0);
    lv_obj_align(g_date_label, LV_ALIGN_CENTER, 0, 25);

    // 星期
    g_weekday_label = lv_label_create(main_frame);
    lv_label_set_text(g_weekday_label, "3");
    lv_obj_set_style_text_color(g_weekday_label, lv_color_hex(color), 0);
    lv_obj_set_style_text_font(g_weekday_label, &lv_font_montserrat_14, 0);
    lv_obj_align(g_weekday_label, LV_ALIGN_CENTER, 0, 45);

    // 底部左点
    lv_obj_t *dot_left = lv_obj_create(main_frame);
    lv_obj_set_size(dot_left, 10, 10);
    lv_obj_align(dot_left, LV_ALIGN_BOTTOM_LEFT, 25, -15);
    lv_obj_set_style_bg_color(dot_left, lv_color_hex(color), 0);
    lv_obj_set_style_border_width(dot_left, 0, 0);
    lv_obj_set_style_radius(dot_left, 5, 0);

    // 底部右点
    lv_obj_t *dot_right = lv_obj_create(main_frame);
    lv_obj_set_size(dot_right, 10, 10);
    lv_obj_align(dot_right, LV_ALIGN_BOTTOM_RIGHT, -25, -15);
    lv_obj_set_style_bg_color(dot_right, lv_color_hex(color), 0);
    lv_obj_set_style_border_width(dot_right, 0, 0);
    lv_obj_set_style_radius(dot_right, 5, 0);

    update_labels();

    // 创建/更新定时器
    if (g_watch_timer) {
        lv_timer_del(g_watch_timer);
        g_watch_timer = NULL;
    }
    g_watch_timer = lv_timer_create(timer_cb, 1000, NULL);
    
    ESP_LOGI(TAG, "Watch UI created with color 0x%06X", color);
}

// 初始化显示 UI 模块
void display_ui_init(void)
{
    init_time();
    ESP_LOGI(TAG, "Display UI initialized");
}