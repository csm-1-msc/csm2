# 重力控制模块 (Gravity Control)

## 功能说明

由于 ESP32-S3-EYE 开发板**没有内置 IMU 传感器**，本模块提供按键控制方式来切换流体重力方向。

## 使用方法

1. **按下按钮**切换到流体动画模式（第 4 种样式）
2. **在流体模式下再次按下按钮**，重力方向会循环切换：
   - 向下 (DOWN) → 向右 (RIGHT) → 向上 (UP) → 向左 (LEFT) → 向下...

## 重力方向示意图

```
正常摆放 (DOWN):     顺时针 90°(RIGHT):    倒置 (UP):          逆时针 90°(LEFT):
    ↑                    →                    ↑                    ←
    |                    |                    |                    |
    ↓                    ↓                    ↑                    ↓
流体向下流动          流体向右流动         流体向上流动         流体向左流动
```

## API 接口

```c
// 初始化重力控制模块
void gravity_control_init(void);

// 获取重力向量 (x, y) - 供流体动画使用
void gravity_control_get_vector(float *gx, float *gy);

// 切换到下一个重力方向
void gravity_control_next_direction(void);

// 设置重力方向
void gravity_control_set_direction(gravity_direction_t dir);

// 获取当前重力方向
gravity_direction_t gravity_control_get_direction(void);

// 设置重力强度 (0.05 - 0.5)
void gravity_control_set_strength(float strength);
```

## 枚举类型

```c
typedef enum {
    GRAVITY_DOWN = 0,   // 向下（默认）
    GRAVITY_RIGHT,      // 向右
    GRAVITY_UP,         // 向上
    GRAVITY_LEFT,       // 向左
    GRAVITY_COUNT
} gravity_direction_t;
```

## 注意事项

- ESP32-S3-EYE 无内置 IMU，无法自动检测旋转角度
- 如需自动检测方向，需要外接 IMU 传感器（如 MPU6050/MPU6886）
- 重力强度默认值为 0.25，可在 0.05-0.5 范围内调整