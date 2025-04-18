#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdbool.h>
#include <fcntl.h>
#include "um_gpio.h"
#include "um_pwm.h"
#include "fan_control.h"

// 电机控制状态
static bool isMotorInitialized = false;
static MotorDirection currentDirection = MOTOR_STOP;
static int currentSpeed = 0;

// 初始化TB6612电机控制器
int initMotorControl() {
    int ret = 0;
    int status = 0;

    // 检查IN1引脚是否已导出
    ret = UM_GPIO_IsExport(MOTOR_IN1_PIN, &status);
    if (ret < 0) {
        printf("Failed to check IN1 GPIO export status, error: %d\n", ret);
        return -1;
    }
    
    // 只有未导出时才导出IN1引脚
    if (status != UM_GPIO_EXPORTED) {
        ret = UM_GPIO_Export(MOTOR_IN1_PIN, 1);
        if (ret < 0) {
            printf("Failed to export IN1 GPIO pin %d, error: %d\n", MOTOR_IN1_PIN, ret);
            return -1;
        }
    }

    // 检查IN2引脚是否已导出
    ret = UM_GPIO_IsExport(MOTOR_IN2_PIN, &status);
    if (ret < 0) {
        printf("Failed to check IN2 GPIO export status, error: %d\n", ret);
        return -1;
    }
    
    // 只有未导出时才导出IN2引脚
    if (status != UM_GPIO_EXPORTED) {
        ret = UM_GPIO_Export(MOTOR_IN2_PIN, 1);
        if (ret < 0) {
            printf("Failed to export IN2 GPIO pin %d, error: %d\n", MOTOR_IN2_PIN, ret);
            return -1;
        }
    }

    // 设置GPIO为输出模式
    ret = UM_GPIO_SetDirection(MOTOR_IN1_PIN, UM_GPIO_DIRECTION_OUT);
    if (ret < 0) {
        printf("Failed to set direction for IN1 GPIO pin %d, error: %d\n", MOTOR_IN1_PIN, ret);
        return -1;
    }

    ret = UM_GPIO_SetDirection(MOTOR_IN2_PIN, UM_GPIO_DIRECTION_OUT);
    if (ret < 0) {
        printf("Failed to set direction for IN2 GPIO pin %d, error: %d\n", MOTOR_IN2_PIN, ret);
        return -1;
    }

    // 初始化PWM
    // ret = init_pmw(MOTOR_PWM_CHANNEL);
    // if (ret < 0) {
    //     printf("Failed to initialize PWM channel %d, error: %d\n", MOTOR_PWM_CHANNEL, ret);
    //     return -1;
    // }
    
    // 设置PWM周期 (20000ns = 20ms, 频率约为50Hz)
    ret = set_pwm_period(MOTOR_PWM_CHANNEL, 20000000);
    if (ret < 0) {
        printf("Failed to set PWM period, error: %d\n", ret);
        return -1;
    }
    
    // 设置PWM极性
    ret = set_pwm_polarity(MOTOR_PWM_CHANNEL, PWM_POLARITY_NORMAL);
    if (ret < 0) {
        printf("Failed to set PWM polarity, error: %d\n", ret);
        return -1;
    }
    
    // 停止电机
    // ret = stopMotor();
    // if (ret < 0) {
    //     printf("Failed to stop motor during initialization, error: %d\n", ret);
    //     return -1;
    // }
    
    // 启用PWM
    ret = set_pwm_enable(MOTOR_PWM_CHANNEL, 1);
    if (ret < 0) {
        printf("Failed to enable PWM, error: %d\n", ret);
        return -1;
    }
    
    isMotorInitialized = true;
    // printf("TB6612 motor controller initialized successfully\n");  // 注释掉成功的输出信息
    return 0;
}

// 设置电机速度 (0-100%)
int setMotorSpeed(int speed) {
    if (!isMotorInitialized) {
        printf("Motor not initialized. Call initMotorControl() first.\n");
        return -1;
    }
    
    if (speed > 100) {
        speed = 100;
    }
    
    // 根据百分比计算PWM占空比值
    // PWM周期为20000000ns，计算对应的占空比值
    int period = get_pwm_period(MOTOR_PWM_CHANNEL);
    if (period < 0) {
        printf("Failed to get PWM period, error: %d\n", period);
        return -1;
    }
    
    int dutyCycle = (period * speed) / 100;
    int ret = set_pwm_dutyCycle(MOTOR_PWM_CHANNEL, dutyCycle);
    if (ret < 0) {
        printf("Failed to set PWM duty cycle, error: %d\n", ret);
        return -1;
    }
    
    currentSpeed = speed;
    return 0;
}

// 设置电机方向
int setMotorDirection(MotorDirection direction) {
    if (!isMotorInitialized) {
        printf("Motor not initialized. Call initMotorControl() first.\n");
        return -1;
    }
    
    int ret = 0;
    
    switch (direction) {
        case MOTOR_FORWARD:
            // 正转: IN1=HIGH, IN2=LOW
            ret = UM_GPIO_SetValue(MOTOR_IN1_PIN, UM_GPIO_HIGH_LEVE);
            if (ret < 0) return ret;
            ret = UM_GPIO_SetValue(MOTOR_IN2_PIN, UM_GPIO_LOW_LEVE);
            break;
            
        case MOTOR_BACKWARD:
            // 反转: IN1=LOW, IN2=HIGH
            ret = UM_GPIO_SetValue(MOTOR_IN1_PIN, UM_GPIO_LOW_LEVE);
            if (ret < 0) return ret;
            ret = UM_GPIO_SetValue(MOTOR_IN2_PIN, UM_GPIO_HIGH_LEVE);
            break;
            
        case MOTOR_STOP:
            // 停止: IN1=LOW, IN2=LOW
            ret = UM_GPIO_SetValue(MOTOR_IN1_PIN, UM_GPIO_LOW_LEVE);
            if (ret < 0) return ret;
            ret = UM_GPIO_SetValue(MOTOR_IN2_PIN, UM_GPIO_LOW_LEVE);
            break;
            
        case MOTOR_BRAKE:
            // 刹车: IN1=HIGH, IN2=HIGH
            ret = UM_GPIO_SetValue(MOTOR_IN1_PIN, UM_GPIO_HIGH_LEVE);
            if (ret < 0) return ret;
            ret = UM_GPIO_SetValue(MOTOR_IN2_PIN, UM_GPIO_HIGH_LEVE);
            break;
            
        default:
            printf("Invalid motor direction\n");
            return -1;
    }
    
    if (ret < 0) {
        printf("Failed to set motor direction pins, error: %d\n", ret);
        return ret;
    }
    
    currentDirection = direction;
    return 0;
}

// 控制电机 (结合速度和方向)
int controlMotor(MotorDirection direction, int speed) {
    if (!isMotorInitialized) {
        printf("Motor not initialized. Call initMotorControl() first.\n");
        return -1;
    }
    
    int ret = setMotorDirection(direction);
    if (ret < 0) {
        return ret;
    }
    
    // 如果是停止或刹车，设置速度为0
    if (direction == MOTOR_STOP) {
        speed = 0;
    }
    
    ret = setMotorSpeed(speed);
    if (ret < 0) {
        return ret;
    }
    
    return 0;
}

// 停止电机
int stopMotor() {
    return controlMotor(MOTOR_STOP, 0);
}
