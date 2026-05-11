# 3_color_control - 基础颜色切换模块
# 包含：4 种颜色定义、颜色切换逻辑

idf_component_register(
    SRCS "color_control.c"
    INCLUDE_DIRS "."
    PRIV_REQUIRES lvgl
)