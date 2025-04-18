#ifndef FAN_CONTROL_H
#define FAN_CONTROL_H

#include <stdint.h>

// TB6612电机控制引脚定义
#define MOTOR_IN1_PIN 383  // GPIO383 用于控制方向
#define MOTOR_IN2_PIN 384  // GPIO384 用于控制方向
#define MOTOR_PWM_CHANNEL 2  // PWM2 用于控制速度

// 电机方向枚举
typedef enum {
    MOTOR_FORWARD = 0,     // 正转
    MOTOR_BACKWARD = 1,    // 反转
    MOTOR_STOP = 2,        // 停止
    MOTOR_BRAKE = 3        // 刹车
} MotorDirection;

// 初始化TB6612电机控制器
int initMotorControl();

// 设置电机速度 (0-100%)
int setMotorSpeed(int speed);

// 设置电机方向
int setMotorDirection(MotorDirection direction);

// 控制电机 (结合速度和方向)
int controlMotor(MotorDirection direction, int speed);

// 停止电机
int stopMotor();

#endif // FAN_CONTROL_H
