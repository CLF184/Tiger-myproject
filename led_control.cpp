#include "led_control.h"
#include "um_gpio.h"
#include <stdio.h>

/**
 * @brief 初始化LED
 * 
 * @return int 成功返回LED_OK，失败返回LED_ERROR
 */
int LedInit(void)
{
    int status = 0;
    
    // 检查GPIO是否已导出
    if (UM_GPIO_IsExport(LED_GPIO, &status) < 0) {
        printf("LED initialization failed: Unable to check GPIO export status\n");
        return LED_ERROR;
    }
    
    // 只有在未导出时才进行导出
    if (status != UM_GPIO_EXPORTED) {
        // 导出GPIO
        if (UM_GPIO_Export(LED_GPIO, UM_GPIO_EXPORTED) < 0) {
            printf("LED initialization failed: Unable to export GPIO\n");
            return LED_ERROR;
        }
    }
    
    // 设置GPIO为输出方向
    if (UM_GPIO_SetDirection(LED_GPIO, UM_GPIO_DIRECTION_OUT) < 0) {
        printf("LED initialization failed: Unable to set GPIO direction\n");
        return LED_ERROR;
    }
    
    // 默认关闭LED
    if (UM_GPIO_SetValue(LED_GPIO, UM_GPIO_LOW_LEVE) < 0) {
        printf("LED initialization failed: Unable to set GPIO default value\n");
        return LED_ERROR;
    }
    
    // printf("LED initialization successful\n");
    return LED_OK;
}

/**
 * @brief 打开LED
 * 
 * @return int 成功返回LED_OK，失败返回LED_ERROR
 */
int LedOn(void)
{
    int status = 0;
    
    // 检查GPIO是否已导出
    if (UM_GPIO_IsExport(LED_GPIO, &status) < 0 || status != UM_GPIO_EXPORTED) {
        printf("Failed to turn on LED: GPIO not exported\n");
        return LED_ERROR;
    }
    
    // 设置GPIO为高电平，点亮LED
    if (UM_GPIO_SetValue(LED_GPIO, UM_GPIO_HIGH_LEVE) < 0) {
        printf("Failed to turn on LED: Unable to set GPIO high level\n");
        return LED_ERROR;
    }
    
    // printf("LED turned on\n");
    return LED_OK;
}

/**
 * @brief 关闭LED
 * 
 * @return int 成功返回LED_OK，失败返回LED_ERROR
 */
int LedOff(void)
{
    int status = 0;
    
    // 检查GPIO是否已导出
    if (UM_GPIO_IsExport(LED_GPIO, &status) < 0 || status != UM_GPIO_EXPORTED) {
        printf("Failed to turn off LED: GPIO not exported\n");
        return LED_ERROR;
    }
    
    // 设置GPIO为低电平，关闭LED
    if (UM_GPIO_SetValue(LED_GPIO, UM_GPIO_LOW_LEVE) < 0) {
        printf("Failed to turn off LED: Unable to set GPIO low level\n");
        return LED_ERROR;
    }
    
    // printf("LED turned off\n");
    return LED_OK;
}

/**
 * @brief 获取LED当前状态
 * 
 * @param status 用于存储LED状态的指针（LED_ON或LED_OFF）
 * @return int 成功返回LED_OK，失败返回LED_ERROR
 */
int LedGetStatus(int *status)
{
    int exportStatus = 0;
    int value = 0;
    
    if (status == NULL) {
        printf("Failed to get LED status: Invalid parameter\n");
        return LED_ERROR;
    }
    
    // 检查GPIO是否已导出
    if (UM_GPIO_IsExport(LED_GPIO, &exportStatus) < 0 || exportStatus != UM_GPIO_EXPORTED) {
        printf("Failed to get LED status: GPIO not exported\n");
        return LED_ERROR;
    }
    
    // 获取GPIO当前值
    if (UM_GPIO_GetValue(LED_GPIO, &value) < 0) {
        printf("Failed to get LED status: Unable to read GPIO value\n");
        return LED_ERROR;
    }
    
    *status = (value == UM_GPIO_HIGH_LEVE) ? LED_ON : LED_OFF;
    return LED_OK;
}

/**
 * @brief 翻转LED状态
 * 
 * @return int 成功返回LED_OK，失败返回LED_ERROR
 */
int LedToggle(void)
{
    int status = 0;
    
    // 获取当前LED状态
    if (LedGetStatus(&status) != LED_OK) {
        return LED_ERROR;
    }
    
    // 根据当前状态切换LED
    if (status == LED_ON) {
        return LedOff();
    } else {
        return LedOn();
    }
}

/**
 * @brief 释放LED资源
 * 
 * @return int 成功返回LED_OK，失败返回LED_ERROR
 */
int LedDeinit(void)
{
    // 关闭LED
    LedOff();
    
    // 取消导出GPIO
    if (UM_GPIO_Export(LED_GPIO, UM_GPIO_NOT_EXPORT) < 0) {
        printf("LED resource release failed: Unable to unexport GPIO\n");
        return LED_ERROR;
    }
    
    // printf("LED resources released\n");
    return LED_OK;
}
