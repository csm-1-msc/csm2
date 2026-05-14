/*
 * 6_gravity_control - 重力方向控制模块
 * @brief 根据开发板姿态改变流体方向
 * 
 * 本模块提供：
 * - 重力方向设置接口
 * - 加速度计数据读取（用于计算重力方向）
 * - 旋转角度识别
 * - 重力方向映射到流体方向
 * 
 * 注意：ESP32-S3-EYE 无内置 IMU，使用加速度计模拟接口
 * 后续可接入外部 IMU 传感器（如 MPU6886/MPU6050）
 */

#include <stdio.h>
#include <math.h>
#include "esp_log.h"
#include "esp_timer.h"

static const char *TAG = "gravity_control";

// 重力方向枚举
typedef enum {
    GRAVITY_DOWN = 0,   // 向下（默认，板子正常摆放）
    GRAVITY_RIGHT,      // 向右（板子顺时针旋转 90 度）
    GRAVITY_UP,         // 向上（板子旋转 180 度）
    GRAVITY_LEFT,       // 向左（板子逆时针旋转 90 度）
    GRAVITY_NONE        // 无重力（板子平放或角度不明确）
} gravity_direction_t;

// 当前重力方向
static gravity_direction_t g_gravity_dir = GRAVITY_DOWN;

// 重力强度
static float g_gravity_strength = 0.08f;

// 加速度计数据（模拟/待接入真实传感器）
static float g_accel_x = 0.0f;
static float g_accel_y = 0.0f;
static float g_accel_z = 0.0f;

// 角度阈值（用于判断方向）
#define ANGLE_THRESHOLD_DEG 45.0f    // 方向判断阈值
#define GRAVITY_THRESHOLD 0.5f       // 重力检测阈值

// 前一次的重力方向（用于防抖动）
static gravity_direction_t g_prev_gravity_dir = GRAVITY_DOWN;
// 方向稳定计数器
static int g_dir_stable_count = 0;
#define DIR_STABLE_COUNT 5           // 需要连续 5 次相同方向才确认

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

// ============================================================================
// 加速度计数据接口（ESP32-S3-EYE 无内置 IMU，提供模拟接口）
// ============================================================================

/**
 * @brief 设置加速度计数据（由外部传感器或模拟数据调用）
 * @param x X 轴加速度（g 单位）
 * @param y Y 轴加速度（g 单位）
 * @param z Z 轴加速度（g 单位）
 * 
 * 注意：ESP32-S3-EYE 无内置 IMU，此函数用于：
 * 1. 测试：手动设置模拟数据
 * 2. 扩展：接入外部 IMU 传感器后自动调用
 */
void gravity_control_set_accel_data(float x, float y, float z)
{
    g_accel_x = x;
    g_accel_y = y;
    g_accel_z = z;
}

/**
 * @brief 获取当前加速度计数据
 * @param ax X 轴加速度指针（可选）
 * @param ay Y 轴加速度指针（可选）
 * @param az Z 轴加速度指针（可选）
 */
void gravity_control_get_accel_data(float *ax, float *ay, float *az)
{
    if (ax) *ax = g_accel_x;
    if (ay) *ay = g_accel_y;
    if (az) *az = g_accel_z;
}

// ============================================================================
// 旋转角度识别与重力方向计算
// ============================================================================

/**
 * @brief 计算开发板相对于正常摆放的旋转角度
 * @return 旋转角度（度），范围 0-360
 * 
 * 角度定义（从正常摆放开始顺时针）：
 * - 0°: 正常摆放（上北下南）
 * - 90°: 顺时针旋转 90 度（上西下东）
 * - 180°: 倒置（上南下北）
 * - 270°: 逆时针旋转 90 度（上东下西）
 */
float gravity_control_get_rotation_angle(void)
{
    // 使用 atan2 计算加速度向量在 XY 平面的角度
    // atan2 返回弧度，范围 -PI 到 +PI
    float angle_rad = atan2f(g_accel_x, g_accel_y);
    // 转换为角度，范围 -180 到 +180
    float angle_deg = angle_rad * 180.0f / M_PI;
    
    // 转换为 0-360 范围
    if (angle_deg < 0) {
        angle_deg += 360.0f;
    }
    
    return angle_deg;
}

/**
 * @brief 根据加速度计数据计算重力方向
 * @return 计算出的重力方向
 * 
 * 方向判断规则：
 * - 正常摆放（Y 轴向下）：GRAVITY_DOWN
 * - 顺时针 90 度（X 轴向下）：GRAVITY_RIGHT
 * - 倒置 180 度（Y 轴向上）：GRAVITY_UP
 * - 逆时针 90 度（X 轴向上）：GRAVITY_LEFT
 */
static gravity_direction_t calculate_gravity_direction(void)
{
    // 计算加速度向量的大小
    float accel_magnitude = sqrtf(g_accel_x * g_accel_x + 
                                   g_accel_y * g_accel_y + 
                                   g_accel_z * g_accel_z);
    
    // 如果加速度太小，认为没有明确的重力方向
    if (accel_magnitude < GRAVITY_THRESHOLD) {
        return GRAVITY_NONE;
    }
    
    // 计算归一化的加速度分量
    float nx = g_accel_x / accel_magnitude;
    float ny = g_accel_y / accel_magnitude;
    
    // 根据加速度方向判断重力方向
    // ESP32-S3-EYE 坐标系：X 向右，Y 向下，Z 向前（屏幕方向）
    
    if (fabsf(ny) > fabsf(nx)) {
        // Y 轴主导
        if (ny > 0) {
            // Y 轴正向（向下）- 正常摆放
            return GRAVITY_DOWN;
        } else {
            // Y 轴负向（向上）- 倒置
            return GRAVITY_UP;
        }
    } else {
        // X 轴主导
        if (nx > 0) {
            // X 轴正向（向右）- 顺时针 90 度
            return GRAVITY_RIGHT;
        } else {
            // X 轴负向（向左）- 逆时针 90 度
            return GRAVITY_LEFT;
        }
    }
}

/**
 * @brief 从加速度计数据更新重力方向（带防抖动）
 * 
 * 防抖动机制：
 * - 需要连续 DIR_STABLE_COUNT 次检测到相同方向才确认切换
 * - 防止因轻微晃动导致方向频繁变化
 */
void gravity_control_update_from_accel(void)
{
    gravity_direction_t new_dir = calculate_gravity_direction();
    
    if (new_dir == g_prev_gravity_dir) {
        // 方向相同，增加稳定计数
        g_dir_stable_count++;
    } else {
        // 方向不同，重置计数
        g_dir_stable_count = 1;
        g_prev_gravity_dir = new_dir;
    }
    
    // 如果稳定计数达到阈值，确认方向切换
    if (g_dir_stable_count >= DIR_STABLE_COUNT) {
        if (new_dir != g_gravity_dir) {
            g_gravity_dir = new_dir;
            ESP_LOGI(TAG, "Gravity direction changed to: %s (angle: %.1f°)", 
                     gravity_control_get_direction_name(new_dir), 
                     gravity_control_get_rotation_angle());
        }
        g_dir_stable_count = 0;  // 重置计数
    }
}

/**
 * @brief 获取当前旋转角度对应的重力方向（无防抖动，用于调试）
 * @param angle 旋转角度（度）
 * @return 对应的重力方向
 */
gravity_direction_t gravity_control_get_direction_from_angle(float angle)
{
    // 规范化角度到 0-360
    while (angle < 0) angle += 360.0f;
    while (angle >= 360) angle -= 360.0f;
    
    // 根据角度范围判断方向（每个方向占 90 度，中心点为 0/90/180/270）
    if (angle >= 315 || angle < 45) {
        return GRAVITY_DOWN;      // 0°附近
    } else if (angle >= 45 && angle < 135) {
        return GRAVITY_RIGHT;     // 90°附近
    } else if (angle >= 135 && angle < 225) {
        return GRAVITY_UP;        // 180°附近
    } else {
        return GRAVITY_LEFT;      // 270°附近
    }
}

// ============================================================================
// 流体重力方向同步接口
// ============================================================================

/**
 * @brief 将当前重力方向同步到流体动画模块
 * 
 * 此函数应被流体动画模块调用，以获取当前的重力向量
 */
void gravity_control_apply_to_fluid(void)
{
    float gx, gy;
    gravity_control_get_vector(&gx, &gy);
    
    // 调用流体模块的重力设置函数
    extern void fluid_set_gravity(float x, float y);
    fluid_set_gravity(gx, gy);
}
