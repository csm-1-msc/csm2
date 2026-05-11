# 4_mode_switch - 5 档按键循环逻辑模块
# 包含：模式状态定义、循环切换逻辑

idf_component_register(
    SRCS "mode_switch.c"
    INCLUDE_DIRS "."
    PRIV_REQUIRES lvgl
)