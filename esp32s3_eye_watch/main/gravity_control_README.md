# 重力方向控制模块 (gravity_control)

## 概述

本模块提供模拟重力方向控制功能，参考 ESP32-S3-EYE 的 `display_rotation` 实现，无需外接传感器即可模拟重力方向变化。

## 功能特性

- **模拟重力方向**：支持四个方向（向下、向右、向上、向左）
- **防抖动机制**：参考 `display_rotation` 的 `ACCEL_ROTATION_HOLD_MS` (350ms) 实现
- **自动检测**：可启用模拟加速度计自动检测方向
- **流体动画集成**：为流体粒子动画提供重力向量

## 参考实现

本模块参考了 ESP32-S3-EYE 的 `display_rotation` 实现：
- 防抖动机制：`ACCEL_ROTATION_HOLD_MS = 350ms`
- 加速度阈值：`ACCEL_ROTATION_THRESHOLD_G = 0.75g`
- 自动检测间隔：`AUTO_DETECT_INTERVAL_MS = 200ms`

## 重力方向枚举

```c
typedef enum {
    GRAVITY_DOWN = 0,   // 向下（默认，板子正常摆放）
    GRAVITY_RIGHT,      // 向右（板子顺时针旋转 90 度）
    GRAVITY_UP,         // 向上（板子旋转 180 度）
    GRAVITY_LEFT,       // 向左（板子逆时针旋转 90 度）
    GRAVITY_COUNT
} gravity_direction_t;
```

## API 接口

### 初始化
```c
void gravity_control_init(void);
```

### 重力向量
```c
void gravity_control_get_vector(float *gx, float *gy);
```

### 方向控制
```c
void gravity_control_next_direction(void);
void gravity_control_set_direction(gravity_direction_t dir);
gravity_direction_t gravity_control_get_direction(void);
```

### 强度控制
```c
void gravity_control_set_strength(float strength);  // 范围：0.05 - 0.5
float gravity_control_get_strength(void);
```

### 自动检测
```c
void gravity_control_enable_auto_detect(bool enable);
bool gravity_control_is_auto_detect_enabled(void);
void gravity_control_update_from_accelerometer(void);
```

### 调试接口
```c
void gravity_control_set_simulated_accel(float x, float y, float z);
void gravity_control_get_simulated_accel(float *ax, float *ay, float *az);
```

## 使用示例

```c
#include "gravity_control.h"

// 初始化
gravity_control_init();

// 启用自动检测
gravity_control_enable_auto_detect(true);

// 在定时任务中定期调用（每 200ms）
void gravity_detect_task(void *arg) {
    while (1) {
        gravity_control_update_from_accelerometer();
        vTaskDelay(pdMS_TO_TICKS(200));
    }
}

// 获取重力向量（用于流体动画）
float gx, gy;
gravity_control_get_vector(&gx, &gy);

// 切换方向（按键触发）
gravity_control_next_direction();
```

## 与流体动画集成

流体动画模块 (`5_fluid_animation/fluid_animation.c`) 会自动从 `gravity_control` 模块获取重力向量：

```c
void fluid_update(void) {
    // 从 gravity_control 模块获取当前重力向量
    gravity_control_get_vector(&g_gravity_x, &g_gravity_y);
    
    // 应用重力到粒子
    g_pvx[i] += g_gravity_x;
    g_pvy[i] += g_gravity_y;
}
```

## 模拟加速度计数据

模块内部模拟了四个方向的加速度计读数：

| 方向 | X 轴 | Y 轴 | Z 轴 |
|------|-----|-----|-----|
| DOWN | 0.0 | 1.0 | 0.0 |
| RIGHT | 1.0 | 0.0 | 0.0 |
| UP | 0.0 | -1.0 | 0.0 |
| LEFT | -1.0 | 0.0 | 0.0 |

## 文件结构

```
main/
├── gravity_control.c       # 重力控制模块实现
├── gravity_control.h       # 重力控制模块头文件
└── gravity_control_README.md  # 本说明文档
```

## 注意事项

1. 本模块使用模拟加速度计数据，无需实际连接传感器
2. 自动检测功能需要配合定时任务使用
3. 防抖动机制确保方向切换稳定
4. 重力强度范围建议保持在 0.05 - 0.5 之间