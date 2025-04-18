#include <stdio.h>
#include "sg90.h"
#include "um_pwm.h"

// 使用PWM1通道控制舵机
#define SG90_PWM_CHANNEL 1

/**
 * 初始化SG90舵机
 * 返回值：成功返回>=0，失败返回<0
 */
int SG90_Init(void)
{
    int ret;
    
    // 初始化PWM通道
    // ret = init_pmw(SG90_PWM_CHANNEL);
    // if (ret < 0) {
    //     printf("Failed to initialize PWM channel %d, error: %d\n", SG90_PWM_CHANNEL, ret);
    //     return ret;
    // }
    
    // 设置PWM周期为20ms (20,000,000 ns)
    ret = set_pwm_period(SG90_PWM_CHANNEL, SG90_PWM_PERIOD);
    if (ret < 0) {
        printf("Failed to set PWM period, error: %d\n", ret);
        return ret;
    }
    
    // 设置PWM极性为正常
    ret = set_pwm_polarity(SG90_PWM_CHANNEL, PWM_POLARITY_NORMAL);
    if (ret < 0) {
        printf("Failed to set PWM polarity, error: %d\n", ret);
        return ret;
    }
    
    // 初始位置设为中间位置（90度）
    ret = SG90_SetAngle(90);
    if (ret < 0) {
        printf("Failed to set initial angle, error: %d\n", ret);
        return ret;
    }
    
    // 启用PWM
    ret = set_pwm_enable(SG90_PWM_CHANNEL, PWM_IS_ENABLED);
    if (ret < 0) {
        printf("Failed to enable PWM, error: %d\n", ret);
        return ret;
    }
    
    // printf("SG90 servo initialized on PWM channel %d\n", SG90_PWM_CHANNEL);
    return 0;
}

/**
 * 设置SG90舵机角度
 * @param angle: 角度值（0-180）
 * 返回值：成功返回>=0，失败返回<0
 */
int SG90_SetAngle(int angle)
{
    int ret;
    int duty_cycle;
    
    // 限制角度范围
    if (angle < SG90_MIN_ANGLE) {
        angle = SG90_MIN_ANGLE;
    } else if (angle > SG90_MAX_ANGLE) {
        angle = SG90_MAX_ANGLE;
    }
    
    // 计算角度对应的脉冲宽度
    // 从0.5ms (SG90_MIN_PULSE) 到 2.5ms (SG90_MAX_PULSE)
    duty_cycle = SG90_MIN_PULSE + (angle * (SG90_MAX_PULSE - SG90_MIN_PULSE) / SG90_MAX_ANGLE);
    
    // 设置PWM占空比
    ret = set_pwm_dutyCycle(SG90_PWM_CHANNEL, duty_cycle);
    if (ret < 0) {
        printf("Failed to set PWM duty cycle, error: %d\n", ret);
        return ret;
    }
    
    // printf("SG90 servo angle set to %d degrees (pulse width: %d ns)\n", angle, duty_cycle);
    return 0;
}

/**
 * 关闭SG90舵机控制（禁用PWM）
 * 返回值：成功返回>=0，失败返回<0
 */
int SG90_Close(void)
{
    int ret;
    
    // 禁用PWM
    ret = set_pwm_enable(SG90_PWM_CHANNEL, PWM_NOT_ENABLED);
    if (ret < 0) {
        printf("Failed to disable PWM, error: %d\n", ret);
        return ret;
    }
    
    // printf("SG90 servo control disabled\n");
    return 0;
}
