/*
 * gravity_control - 重力方向控制模块头文件
 * @brief 模拟重力方向控制（无需外接传感器）
 * 
 * 参考 display_rotation 实现，提供：
 * - 模拟重力方向（无需外接传感器）
 * - 按键切换重力方向
 * - 模拟重力向量供流体动画使用
 * - 防抖动机制（参考 display_rotation 的 ACCEL_ROTATION_HOLD_MS）
 */

#ifndef GRAVITY_CONTROL_H
#define GRAVITY_CONTROL_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// 重力方向枚举
typedef enum {
    GRAVITY_DOWN = 0,   // 向下（默认，板子正常摆放）
    GRAVITY_RIGHT,      // 向右（板子顺时针旋转 90 度）
    GRAVITY_UP,         // 向上（板子旋转 180 度）
    GRAVITY_LEFT,       // 向左（板子逆时针旋转 90 度）
    GRAVITY_COUNT
} gravity_direction_t;

/**
 * @brief 初始化重力控制模块
 */
void gravity_control_init(void);

/**
 * @brief 获取重力向量 (x, y)
 * @param gx X 轴重力分量指针
 * @param gy Y 轴重力分量指针
 */
void gravity_control_get_vector(float *gx, float *gy);

/**
 * @brief 切换到下一个重力方向（循环：DOWN→RIGHT→UP→LEFT→DOWN）
 */
void gravity_control_next_direction(void);

/**
 * @brief 设置重力方向
 * @param dir 重力方向
 */
void gravity_control_set_direction(gravity_direction_t dir);

/**
 * @brief 获取当前重力方向
 * @return 当前重力方向
 */
gravity_direction_t gravity_control_get_direction(void);

/**
 * @brief 设置重力强度 (0.05 - 0.5)
 * @param strength 重力强度
 */
void gravity_control_set_strength(float strength);

/**
 * @brief 获取重力强度
 * @return 当前重力强度
 */
float gravity_control_get_strength(void);

/**
 * @brief 启用模拟加速度计自动检测方向
 * @param enable true 启用，false 禁用（使用手动设置的方向）
 * 
 * 启用后，模块会周期性地模拟加速度计数据并自动检测重力方向
 * 参考 display_rotation 的 ACCEL_ROTATION_HOLD_MS 防抖动机制
 */
void gravity_control_enable_auto_detect(bool enable);

/**
 * @brief 检查是否启用模拟加速度计自动检测
 * @return true 启用，false 禁用
 */
bool gravity_control_is_auto_detect_enabled(void);

/**
 * @brief 从模拟加速度计读取并更新重力方向
 * 
 * 参考 display_rotation 的 app_accel_commit_rotation 实现：
 * - 使用 ACCEL_ROTATION_HOLD_MS (350ms) 防抖动
 * - 只有当方向稳定保持一段时间后才确认切换
 */
void gravity_control_update_from_accelerometer(void);

/**
 * @brief 设置模拟加速度计数据（用于调试）
 * @param x X 轴加速度
 * @param y Y 轴加速度
 * @param z Z 轴加速度
 */
void gravity_control_set_simulated_accel(float x, float y, float z);

/**
 * @brief 获取当前模拟加速度计数据
 * @param ax X 轴加速度指针（可选）
 * @param ay Y 轴加速度指针（可选）
 * @param az Z 轴加速度指针（可选）
 */
void gravity_control_get_simulated_accel(float *ax, float *ay, float *az);

#ifdef __cplusplus
}
#endif

#endif // GRAVITY_CONTROL_H