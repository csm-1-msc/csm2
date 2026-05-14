/*
 * 6_gravity_control - 重力方向控制模块头文件
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

#ifndef GRAVITY_CONTROL_H
#define GRAVITY_CONTROL_H

#ifdef __cplusplus
extern "C" {
#endif

// 重力方向枚举
typedef enum {
    GRAVITY_DOWN = 0,   // 向下（默认，板子正常摆放）
    GRAVITY_RIGHT,      // 向右（板子顺时针旋转 90 度）
    GRAVITY_UP,         // 向上（板子旋转 180 度）
    GRAVITY_LEFT,       // 向左（板子逆时针旋转 90 度）
    GRAVITY_NONE        // 无重力（板子平放或角度不明确）
} gravity_direction_t;

// ============================================================================
// 初始化
// ============================================================================

/**
 * @brief 初始化重力控制模块
 */
void gravity_control_init(void);

// ============================================================================
// 重力方向控制
// ============================================================================

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
 * @brief 获取重力向量 (x, y)
 * @param gx X 轴重力分量指针
 * @param gy Y 轴重力分量指针
 */
void gravity_control_get_vector(float *gx, float *gy);

/**
 * @brief 设置重力强度
 * @param strength 重力强度 (0.0 - 0.5)
 */
void gravity_control_set_strength(float strength);

/**
 * @brief 获取重力强度
 * @return 当前重力强度
 */
float gravity_control_get_strength(void);

/**
 * @brief 获取方向名称（用于调试）
 * @param dir 重力方向
 * @return 方向名称字符串
 */
const char* gravity_control_get_direction_name(gravity_direction_t dir);

/**
 * @brief 循环切换重力方向（用于按键测试）
 */
void gravity_control_cycle_direction(void);

// ============================================================================
// 加速度计数据接口
// ============================================================================

/**
 * @brief 设置加速度计数据（由外部传感器或模拟数据调用）
 * @param x X 轴加速度（g 单位）
 * @param y Y 轴加速度（g 单位）
 * @param z Z 轴加速度（g 单位）
 */
void gravity_control_set_accel_data(float x, float y, float z);

/**
 * @brief 获取当前加速度计数据
 * @param ax X 轴加速度指针（可选）
 * @param ay Y 轴加速度指针（可选）
 * @param az Z 轴加速度指针（可选）
 */
void gravity_control_get_accel_data(float *ax, float *ay, float *az);

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
float gravity_control_get_rotation_angle(void);

/**
 * @brief 从加速度计数据更新重力方向（带防抖动）
 * 
 * 防抖动机制：
 * - 需要连续 5 次检测到相同方向才确认切换
 * - 防止因轻微晃动导致方向频繁变化
 */
void gravity_control_update_from_accel(void);

/**
 * @brief 获取当前旋转角度对应的重力方向（无防抖动，用于调试）
 * @param angle 旋转角度（度）
 * @return 对应的重力方向
 */
gravity_direction_t gravity_control_get_direction_from_angle(float angle);

// ============================================================================
// 流体重力方向同步接口
// ============================================================================

/**
 * @brief 将当前重力方向同步到流体动画模块
 * 
 * 此函数应被流体动画模块调用，以获取当前的重力向量
 */
void gravity_control_apply_to_fluid(void);

#ifdef __cplusplus
}
#endif

#endif // GRAVITY_CONTROL_H