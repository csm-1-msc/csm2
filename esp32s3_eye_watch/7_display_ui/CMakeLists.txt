# 7_display_ui - 显示/UI 界面模块
# 包含：Watch UI 创建、时间显示、日期显示、UI 组件管理

idf_component_register(
    SRCS "display_ui.c"
    INCLUDE_DIRS "."
    PRIV_REQUIRES lvgl esp_timer
)