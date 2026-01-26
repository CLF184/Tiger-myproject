#include <stdio.h>
#include <unistd.h>
#include "buzzer_control.h"
#include "um_gpio.h"

// 蜂鸣器使用的GPIO引脚
#define BUZZER_GPIO_PIN 382

/**
 * @brief 初始化蜂鸣器
 * 
 * @return int 成功返回0，失败返回负值
 */
int BuzzerInit(void)
{
    int ret;
    int status;
    
    // 检查GPIO引脚是否已导出
    ret = UM_GPIO_IsExport(BUZZER_GPIO_PIN, &status);
    if (ret < 0) {
        printf("Failed to check GPIO %d export status, ret = %d\n", BUZZER_GPIO_PIN, ret);
        return -1;
    }
    
    // 只有在未导出的情况下才进行导出
    if (status != UM_GPIO_EXPORTED) {
        // 导出GPIO引脚
        ret = UM_GPIO_Export(BUZZER_GPIO_PIN, UM_GPIO_EXPORTED);
        if (ret < 0) {
            printf("Failed to export GPIO %d, ret = %d\n", BUZZER_GPIO_PIN, ret);
            return -1;
        }
    }
    
    // 设置为输出方向
    ret = UM_GPIO_SetDirection(BUZZER_GPIO_PIN, UM_GPIO_DIRECTION_OUT);
    if (ret < 0) {
        printf("Failed to set GPIO %d direction, ret = %d\n", BUZZER_GPIO_PIN, ret);
        return -2;
    }
    
    // 初始状态设为低电平（蜂鸣器关闭）
    ret = UM_GPIO_SetValue(BUZZER_GPIO_PIN, UM_GPIO_LOW_LEVE);
    if (ret < 0) {
        printf("Failed to set GPIO %d value, ret = %d\n", BUZZER_GPIO_PIN, ret);
        return -3;
    }
    
    return 0;
}

/**
 * @brief 蜂鸣器鸣响
 * 
 * @param on 1-开启，0-关闭
 * @return int 成功返回0，失败返回负值
 */
int BuzzerControl(int on)
{
    int ret;
    int value = on ? UM_GPIO_HIGH_LEVE : UM_GPIO_LOW_LEVE;
    
    // 设置GPIO电平控制蜂鸣器
    ret = UM_GPIO_SetValue(BUZZER_GPIO_PIN, value);
    if (ret < 0) {
        printf("Failed to set GPIO %d value, ret = %d\n", BUZZER_GPIO_PIN, ret);
        return -1;
    }
    
    return 0;
}

/**
 * @brief 蜂鸣器鸣响一段时间
 * 
 * @param milliseconds 持续时间(毫秒)
 * @return int 成功返回0，失败返回负值
 */
int BuzzerBeep(int milliseconds)
{
    int ret;
    
    // 打开蜂鸣器
    ret = BuzzerControl(1);
    if (ret < 0) {
        return ret;
    }
    
    // 等待指定时间
    usleep(milliseconds * 1000);
    
    // 关闭蜂鸣器
    ret = BuzzerControl(0);
    if (ret < 0) {
        return ret;
    }
    
    return 0;
}

/**
 * @brief 释放蜂鸣器资源
 * 
 * @return int 成功返回0，失败返回负值
 */
int BuzzerDeinit(void)
{
    int ret;
    
    // 关闭蜂鸣器
    ret = BuzzerControl(0);
    if (ret < 0) {
        return ret;
    }
    
    // 取消导出GPIO引脚
    ret = UM_GPIO_Export(BUZZER_GPIO_PIN, UM_GPIO_NOT_EXPORT);
    if (ret < 0) {
        printf("Failed to unexport GPIO %d, ret = %d\n", BUZZER_GPIO_PIN, ret);
        return -1;
    }
    
    return 0;
}
