# 1_quickstart - 基础工程框架
# 包含：CMake 配置、main 入口、SDK 配置、初始化

idf_component_register(
    SRCS "main.c"
    INCLUDE_DIRS "."
    PRIV_REQUIRES esp_timer esp_lcd esp_lvgl_port
)