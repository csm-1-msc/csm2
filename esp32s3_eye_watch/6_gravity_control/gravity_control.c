/*
 * 6_gravity_control - 重力方向控制模块（预留结构）
 * @brief 根据开发板姿态改变流体方向
 * 
 * 本模块提供：
 * - 重力方向设置接口
 * - IMU 传感器预留（后续实现）
 * - 方向映射函数
 * 
 * 注意：当前仅创建结构，不修改现有流体代码
 * 后续将添加 IMU 传感器读取和方向控制功能
 */

#include <stdio.h>
#include "esp_log.h"

static const char *TAG = "gravity_control";

// 重力方向枚举
typedef enum {
    GRAVITY_DOWN = 0,   // 向下（默认）
    GRAVITY_UP,         // 向上
    GRAVITY_LEFT,       // 向左
    GRAVITY_RIGHT,      // 向右
    GRAVITY_NONE        // 无重力
} gravity_direction_t;

// 当前重力方向
static gravity_direction_t g_gravity_dir = GRAVITY_DOWN;

// 重力强度
static float g_gravity_strength = 0.08f;

// 初始化重力控制模块
void gravity_control_init(void)
{
    g_gravity_dir = GRAVITY_DOWN;
    g_gravity_strength = 0.08f;
    ESP_LOGI(TAG, "Gravity control initialized (direction: DOWN)");
}

// 设置重力方向
void gravity_control_set_direction(gravity_direction_t dir)
{
    g_gravity_dir = dir;
    ESP_LOGI(TAG, "Gravity direction set to: %d", dir);
}

// 获取当前重力方向
gravity_direction_t gravity_control_get_direction(void)
{
    return g_gravity_dir;
}

// 获取重力向量 (x, y)
void gravity_control_get_vector(float *gx, float *gy)
{
    if (gx == NULL || gy == NULL) return;
    
    switch (g_gravity_dir) {
        case GRAVITY_DOWN:
            *gx = 0.0f;
            *gy = g_gravity_strength;
            break;
        case GRAVITY_UP:
            *gx = 0.0f;
            *gy = -g_gravity_strength;
            break;
        case GRAVITY_LEFT:
            *gx = -g_gravity_strength;
            *gy = 0.0f;
            break;
        case GRAVITY_RIGHT:
            *gx = g_gravity_strength;
            *gy = 0.0f;
            break;
        case GRAVITY_NONE:
            *gx = 0.0f;
            *gy = 0.0f;
            break;
    }
}

// 设置重力强度
void gravity_control_set_strength(float strength)
{
    if (strength < 0.0f) strength = 0.0f;
    if (strength > 0.5f) strength = 0.5f;
    g_gravity_strength = strength;
    ESP_LOGI(TAG, "Gravity strength set to: %.2f", g_gravity_strength);
}

// 获取重力强度
float gravity_control_get_strength(void)
{
    return g_gravity_strength;
}

// 获取方向名称（用于调试）
const char* gravity_control_get_direction_name(gravity_direction_t dir)
{
    switch (dir) {
        case GRAVITY_DOWN:   return "DOWN";
        case GRAVITY_UP:     return "UP";
        case GRAVITY_LEFT:   return "LEFT";
        case GRAVITY_RIGHT:  return "RIGHT";
        case GRAVITY_NONE:   return "NONE";
        default:             return "UNKNOWN";
    }
}

// 循环切换重力方向（用于按键测试）
void gravity_control_cycle_direction(void)
{
    g_gravity_dir = (gravity_direction_t)((g_gravity_dir + 1) % 5);
    ESP_LOGI(TAG, "Gravity direction cycled to: %s", gravity_control_get_direction_name(g_gravity_dir));
}

// IMU 传感器初始化（预留，后续实现）
// void gravity_control_imu_init(void)
// {
//     // TODO: 初始化 IMU 传感器
//     // ESP_LOGI(TAG, "IMU sensor initialized");
// }

// 读取 IMU 并更新重力方向（预留，后续实现）
// void gravity_control_update_from_imu(void)
// {
//     // TODO: 从 IMU 读取加速度数据并计算重力方向
// }