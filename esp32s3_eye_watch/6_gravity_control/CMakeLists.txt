# 6_gravity_control - 重力方向控制模块（预留结构）
# 包含：重力方向接口、IMU 传感器预留
# 注意：当前仅创建结构，不修改现有流体代码

idf_component_register(
    SRCS "gravity_control.c"
    INCLUDE_DIRS "."
    PRIV_REQUIRES driver
)