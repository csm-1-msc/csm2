# 2_key_driver - 按键驱动模块
# 包含：按键 GPIO 初始化、ISR 处理、信号量机制

idf_component_register(
    SRCS "key_driver.c"
    INCLUDE_DIRS "."
    PRIV_REQUIRES driver
)