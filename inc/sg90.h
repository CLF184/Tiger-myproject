#ifndef SG90_H
#define SG90_H

// SG90舵机角度范围
#define SG90_MIN_ANGLE 0
#define SG90_MAX_ANGLE 180

// SG90舵机PWM控制参数（单位：纳秒）
#define SG90_PWM_PERIOD 20000000  // 20ms = 20,000,000 ns
#define SG90_MIN_PULSE  500000    // 0.5ms = 500,000 ns
#define SG90_MAX_PULSE  2500000   // 2.5ms = 2,500,000 ns

// SG90舵机控制函数
/**
 * 初始化SG90舵机
 * 返回值：成功返回>=0，失败返回<0
 */
int SG90_Init(void);

/**
 * 设置SG90舵机角度
 * @param angle: 角度值（0-180）
 * 返回值：成功返回>=0，失败返回<0
 */
int SG90_SetAngle(int angle);

/**
 * 关闭SG90舵机控制（禁用PWM）
 * 返回值：成功返回>=0，失败返回<0
 */
int SG90_Close(void);

#endif /* SG90_H */
