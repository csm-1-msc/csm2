/*
 * gravity_control - 重力方向控制模块
 * @brief 重力方向控制（使用真实加速度计）
 * 
 * 本模块提供：
 * - 读取真实加速度计数据（QMA6100P）
 * - 根据加速度计数据自动检测重力方向
 * - 按键切换重力方向（备用）
 * - 重力向量接口供流体动画使用
 * - 防抖动机制（参考 display_rotation 的 ACCEL_ROTATION_HOLD_MS）
 */

#include <stdio.h>
#include <math.h>
#include <stdbool.h>
#include "esp_log.h"
#include "esp_timer.h"
#include "driver/i2c_master.h"
#include "qma6100p.h"

static const char *TAG = "gravity_control";

// 重力方向枚举（移到前面以便前向声明使用）
typedef enum {
    GRAVITY_DOWN = 0,   // 向下（默认，板子正常摆放）
    GRAVITY_RIGHT,      // 向右（板子顺时针旋转 90 度）
    GRAVITY_UP,         // 向上（板子旋转 180 度）
    GRAVITY_LEFT,       // 向左（板子逆时针旋转 90 度）
    GRAVITY_COUNT
} gravity_direction_t;

// 前向声明
static void update_accel_from_sensor(void);
void gravity_control_update_from_accelerometer(void);

// 当前重力方向
static gravity_direction_t g_gravity_dir = GRAVITY_DOWN;

// 重力强度（加快流体速度）
static float g_gravity_strength = 0.25f;

// 重力方向名称
static const char *g_dir_names[] = {"DOWN", "RIGHT", "UP", "LEFT"};

// 加速度计相关
static qma6100p_handle_t g_accel_sensor = NULL;
static i2c_master_bus_handle_t g_i2c_bus = NULL;
static bool g_accel_initialized = false;

// 当前加速度计数据
static float g_accel_x = 0.0f;
static float g_accel_y = 1.0f;
static float g_accel_z = 0.0f;

// 防抖动机制（参考 display_rotation）
static gravity_direction_t g_pending_rotation = GRAVITY_DOWN;
static int64_t g_pending_since_us = 0;
#define ACCEL_ROTATION_HOLD_MS 350  // 参考 display_rotation
#define ACCEL_ROTATION_THRESHOLD_G 0.75f  // 参考 display_rotation

// 自动检测相关
static bool g_auto_detect_enabled = false;
static int64_t g_last_detect_time_us = 0;
#define AUTO_DETECT_INTERVAL_MS 200  // 自动检测间隔

// ============================================================================
// 加速度计初始化
// ============================================================================

/**
 * @brief 初始化加速度计（带超时处理）
 */
esp_err_t gravity_control_init_accel(void)
{
    if (g_accel_initialized) {
        return ESP_OK;
    }

    ESP_LOGI(TAG, "Initializing I2C bus for QMA6100P...");
    
    // 创建 I2C 总线（带超时配置）
    const i2c_master_bus_config_t i2c_bus_cfg = {
        .i2c_port = I2C_NUM_0,
        .scl_io_num = 18,  // ESP32-S3-EYE I2C SCL
        .sda_io_num = 17,  // ESP32-S3-EYE I2C SDA
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags = {
            .enable_internal_pullup = 1,  // 启用内部上拉
        },
    };
    
    esp_err_t ret = i2c_new_master_bus(&i2c_bus_cfg, &g_i2c_bus);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create I2C bus: %s", esp_err_to_name(ret));
        return ret;
    }

    // 创建加速度计设备
    ESP_LOGI(TAG, "Creating QMA6100P sensor device...");
    ret = qma6100p_create(g_i2c_bus, QMA6100P_I2C_ADDRESS, &g_accel_sensor);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create QMA6100P sensor: %s", esp_err_to_name(ret));
        // qma6100p_create 失败时不会添加设备，所以不需要删除
        g_i2c_bus = NULL;
        return ret;
    }

    // 尝试读取设备 ID 验证传感器连接
    ESP_LOGI(TAG, "Verifying sensor connection...");
    uint8_t device_id;
    ret = qma6100p_get_deviceid(g_accel_sensor, &device_id);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to read device ID: %s", esp_err_to_name(ret));
        qma6100p_delete(g_accel_sensor);
        g_accel_sensor = NULL;
        return ret;
    }
    ESP_LOGI(TAG, "QMA6100P device ID: 0x%02X", device_id);

    // 唤醒传感器
    ESP_LOGI(TAG, "Waking up sensor...");
    ret = qma6100p_wake_up(g_accel_sensor);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Failed to wake up sensor: %s, continuing anyway", esp_err_to_name(ret));
    }

    // 加载 NVM
    ESP_LOGI(TAG, "Loading NVM...");
    ret = qma6100p_nvm_load(g_accel_sensor);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Failed to load NVM: %s", esp_err_to_name(ret));
    }

    // 配置加速度计量程为±2G
    ESP_LOGI(TAG, "Configuring accelerometer range...");
    ret = qma6100p_config(g_accel_sensor, ACCE_FS_2G);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure sensor: %s", esp_err_to_name(ret));
        qma6100p_delete(g_accel_sensor);
        g_accel_sensor = NULL;
        return ret;
    }

    g_accel_initialized = true;
    ESP_LOGI(TAG, "QMA6100P accelerometer initialized successfully");
    
    // 立即读取一次数据
    update_accel_from_sensor();
    
    return ESP_OK;
}

/**
 * @brief 从加速度计读取数据
 */
static void update_accel_from_sensor(void)
{
    if (!g_accel_initialized || g_accel_sensor == NULL) {
        // 如果传感器未初始化，使用默认值
        g_accel_x = 0.0f;
        g_accel_y = 1.0f;
        g_accel_z = 0.0f;
        return;
    }

    qma6100p_acce_value_t acce_value;
    esp_err_t ret = qma6100p_get_acce(g_accel_sensor, &acce_value);
    
    if (ret == ESP_OK) {
        g_accel_x = acce_value.acce_x;
        g_accel_y = acce_value.acce_y;
        g_accel_z = acce_value.acce_z;
        ESP_LOGD(TAG, "Accelerometer: x=%.3f, y=%.3f, z=%.3f", g_accel_x, g_accel_y, g_accel_z);
    } else {
        ESP_LOGW(TAG, "Failed to read accelerometer: %s", esp_err_to_name(ret));
    }
}

// ============================================================================
// 模拟加速度计数据更新（备用，当传感器不可用时）
// ============================================================================

/**
 * @brief 更新模拟加速度计数据
 * @param dir 要模拟的方向
 */
static void update_simulated_accel(gravity_direction_t dir)
{
    switch (dir) {
        case GRAVITY_DOWN:
            g_accel_x = 0.0f;
            g_accel_y = 1.0f;
            g_accel_z = 0.0f;
            break;
        case GRAVITY_RIGHT:
            g_accel_x = 1.0f;
            g_accel_y = 0.0f;
            g_accel_z = 0.0f;
            break;
        case GRAVITY_UP:
            g_accel_x = 0.0f;
            g_accel_y = -1.0f;
            g_accel_z = 0.0f;
            break;
        case GRAVITY_LEFT:
            g_accel_x = -1.0f;
            g_accel_y = 0.0f;
            g_accel_z = 0.0f;
            break;
        default:
            g_accel_x = 0.0f;
            g_accel_y = 1.0f;
            g_accel_z = 0.0f;
            break;
    }
    ESP_LOGD(TAG, "Simulated accel: x=%.2f, y=%.2f, z=%.2f", g_accel_x, g_accel_y, g_accel_z);
}

/**
 * @brief 计算加速度向量的大小
 */
static float calculate_accel_magnitude(void)
{
    return sqrtf(g_accel_x * g_accel_x + g_accel_y * g_accel_y + g_accel_z * g_accel_z);
}

/**
 * @brief 根据加速度计数据计算重力方向（参考 display_rotation 的 app_accel_candidate_rotation）
 */
static gravity_direction_t calculate_gravity_direction_from_accel(void)
{
    float accel_magnitude = calculate_accel_magnitude();
    
    // 如果加速度太小，保持当前方向
    if (accel_magnitude < ACCEL_ROTATION_THRESHOLD_G) {
        return g_gravity_dir;
    }
    
    // 计算归一化的加速度分量
    float nx = g_accel_x / accel_magnitude;
    float ny = g_accel_y / accel_magnitude;
    float nz = g_accel_z / accel_magnitude;
    
    // 根据加速度方向判断重力方向（参考 display_rotation 的逻辑）
    float abs_x = fabsf(nx);
    float abs_y = fabsf(ny);
    float abs_z = fabsf(nz);
    
    if (abs_z > abs_x && abs_z > abs_y) {
        // Z 轴主导
        if (nz < -0.5f) {
            return GRAVITY_DOWN;
        } else if (nz > 0.5f) {
            return GRAVITY_UP;
        }
    } else if (abs_y > abs_x && abs_y > abs_z) {
        // Y 轴主导
        if (ny < -0.5f) {
            return GRAVITY_RIGHT;  // Y 轴负向对应向右
        } else if (ny > 0.5f) {
            return GRAVITY_LEFT;   // Y 轴正向对应向左
        }
    } else if (abs_x > abs_y && abs_x > abs_z) {
        // X 轴主导
        if (nx < -0.5f) {
            return GRAVITY_LEFT;   // X 轴负向对应向左
        } else if (nx > 0.5f) {
            return GRAVITY_RIGHT;  // X 轴正向对应向右
        }
    }
    
    return g_gravity_dir;
}

// ============================================================================
// 重力控制模块初始化
// ============================================================================

/**
 * @brief 初始化重力控制模块
 */
void gravity_control_init(void)
{
    g_gravity_dir = GRAVITY_DOWN;
    g_gravity_strength = 0.25f;
    g_accel_x = 0.0f;
    g_accel_y = 1.0f;
    g_accel_z = 0.0f;
    g_pending_rotation = GRAVITY_DOWN;
    g_pending_since_us = 0;
    g_last_detect_time_us = 0;
    
    // 尝试初始化加速度计
    if (gravity_control_init_accel() != ESP_OK) {
        ESP_LOGW(TAG, "Accelerometer initialization failed, using simulated mode");
        g_accel_initialized = false;
    }
    
    ESP_LOGI(TAG, "Gravity control initialized (direction: %s, strength: %.2f)", 
             g_dir_names[g_gravity_dir], g_gravity_strength);
}

// ============================================================================
// 重力向量接口
// ============================================================================

/**
 * @brief 获取重力向量 (x, y)
 * @param gx X 轴重力分量指针
 * @param gy Y 轴重力分量指针
 */
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
        default:
            *gx = 0.0f;
            *gy = g_gravity_strength;
            break;
    }
    
    ESP_LOGD(TAG, "Gravity vector: (%.3f, %.3f) - %s", *gx, *gy, g_dir_names[g_gravity_dir]);
}

// ============================================================================
// 重力方向控制
// ============================================================================

/**
 * @brief 切换到下一个重力方向（循环：DOWN→RIGHT→UP→LEFT→DOWN）
 */
void gravity_control_next_direction(void)
{
    g_gravity_dir = (gravity_direction_t)((g_gravity_dir + 1) % GRAVITY_COUNT);
    update_simulated_accel(g_gravity_dir);
    ESP_LOGI(TAG, "Gravity direction changed to: %s (strength: %.2f)", 
             g_dir_names[g_gravity_dir], g_gravity_strength);
}

/**
 * @brief 设置重力方向
 * @param dir 重力方向
 */
void gravity_control_set_direction(gravity_direction_t dir)
{
    if (dir < GRAVITY_COUNT) {
        g_gravity_dir = dir;
        update_simulated_accel(g_gravity_dir);
        ESP_LOGI(TAG, "Gravity direction set to: %s", g_dir_names[g_gravity_dir]);
    }
}

/**
 * @brief 获取当前重力方向
 * @return 当前重力方向
 */
gravity_direction_t gravity_control_get_direction(void)
{
    return g_gravity_dir;
}

// ============================================================================
// 重力强度控制
// ============================================================================

/**
 * @brief 设置重力强度 (0.05 - 0.5)
 * @param strength 重力强度
 */
void gravity_control_set_strength(float strength)
{
    if (strength < 0.05f) strength = 0.05f;
    if (strength > 0.5f) strength = 0.5f;
    g_gravity_strength = strength;
    ESP_LOGI(TAG, "Gravity strength set to: %.2f", g_gravity_strength);
}

/**
 * @brief 获取重力强度
 * @return 当前重力强度
 */
float gravity_control_get_strength(void)
{
    return g_gravity_strength;
}

// ============================================================================
// 加速度计自动检测（参考 display_rotation 的实现）
// ============================================================================

/**
 * @brief 启用加速度计自动检测方向
 * @param enable true 启用，false 禁用（使用手动设置的方向）
 * 
 * 启用后，模块会周期性地读取加速度计数据并自动检测重力方向
 * 参考 display_rotation 的 ACCEL_ROTATION_HOLD_MS 防抖动机制
 */
void gravity_control_enable_auto_detect(bool enable)
{
    g_auto_detect_enabled = enable;
    ESP_LOGI(TAG, "Auto-detect %s", enable ? "enabled" : "disabled");
    
    if (enable) {
        // 立即读取加速度计数据并检测一次方向
        update_accel_from_sensor();
        gravity_control_update_from_accelerometer();
    }
}

/**
 * @brief 检查是否启用加速度计自动检测
 * @return true 启用，false 禁用
 */
bool gravity_control_is_auto_detect_enabled(void)
{
    return g_auto_detect_enabled;
}

/**
 * @brief 从加速度计读取并更新重力方向
 * 
 * 参考 display_rotation 的 app_accel_commit_rotation 实现：
 * - 使用 ACCEL_ROTATION_HOLD_MS (350ms) 防抖动
 * - 只有当方向稳定保持一段时间后才确认切换
 */
void gravity_control_update_from_accelerometer(void)
{
    if (!g_auto_detect_enabled) {
        return;
    }
    
    // 检查是否需要更新（防过度频繁检测）
    int64_t now_us = esp_timer_get_time();
    if ((now_us - g_last_detect_time_us) < (AUTO_DETECT_INTERVAL_MS * 1000)) {
        return;
    }
    g_last_detect_time_us = now_us;
    
    // 从传感器读取数据
    update_accel_from_sensor();
    
    // 根据当前加速度计数据计算候选方向
    gravity_direction_t candidate_dir = calculate_gravity_direction_from_accel();
    
    // 参考 display_rotation 的 commit rotation 逻辑
    if (candidate_dir == g_gravity_dir) {
        // 候选方向与当前方向相同，重置 pending
        g_pending_rotation = g_gravity_dir;
        g_pending_since_us = 0;
        return;
    }
    
    if (candidate_dir != g_pending_rotation) {
        // 新的候选方向，开始计时
        g_pending_rotation = candidate_dir;
        g_pending_since_us = now_us;
        return;
    }
    
    // 检查是否已经保持了足够长的时间
    if ((now_us - g_pending_since_us) < (ACCEL_ROTATION_HOLD_MS * 1000)) {
        return;  // 时间不够，不切换
    }
    
    // 时间足够，确认切换方向
    g_gravity_dir = candidate_dir;
    g_pending_since_us = 0;
    ESP_LOGI(TAG, "Auto-detected gravity direction: %s", g_dir_names[g_gravity_dir]);
}

// ============================================================================
// 调试接口
// ============================================================================

/**
 * @brief 设置模拟加速度计数据（用于调试）
 * @param x X 轴加速度
 * @param y Y 轴加速度
 * @param z Z 轴加速度
 */
void gravity_control_set_simulated_accel(float x, float y, float z)
{
    g_accel_x = x;
    g_accel_y = y;
    g_accel_z = z;
    ESP_LOGD(TAG, "Simulated accel set: x=%.2f, y=%.2f, z=%.2f", x, y, z);
}

/**
 * @brief 获取当前模拟加速度计数据
 * @param ax X 轴加速度指针（可选）
 * @param ay Y 轴加速度指针（可选）
 * @param az Z 轴加速度指针（可选）
 */
void gravity_control_get_simulated_accel(float *ax, float *ay, float *az)
{
    if (ax) *ax = g_accel_x;
    if (ay) *ay = g_accel_y;
    if (az) *az = g_accel_z;
}