# 5_fluid_animation - 流体动画效果模块
# 包含：流体粒子模拟、动画渲染、定时器控制

idf_component_register(
    SRCS "fluid_animation.c"
    INCLUDE_DIRS "."
    PRIV_REQUIRES lvgl esp_timer
)