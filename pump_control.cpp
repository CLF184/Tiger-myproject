#include "pump_control.h"
#include "um_gpio.h"
#include <stdio.h>
#include <unistd.h>

// 使用UM_GPIO_01 (GPIO380)作为水泵控制引脚
#define PUMP_GPIO UM_GPIO_01

int pump_init(void)
{
    int ret;
    int status;
    
    // 检查GPIO是否已导出
    ret = UM_GPIO_IsExport(PUMP_GPIO, &status);
    if (ret < 0) {
        printf("Failed to check pump GPIO export status: %d\n", ret);
        return -1;
    }
    
    // 只有在未导出的情况下才进行导出
    if (status != UM_GPIO_EXPORTED) {
        // 导出GPIO
        ret = UM_GPIO_Export(PUMP_GPIO, UM_GPIO_EXPORTED);
        if (ret < 0) {
            printf("Failed to export pump GPIO: %d\n", ret);
            return -1;
        }
    }
    
    // 设置为输出模式
    ret = UM_GPIO_SetDirection(PUMP_GPIO, UM_GPIO_DIRECTION_OUT);
    if (ret < 0) {
        printf("Failed to set pump GPIO direction: %d\n", ret);
        return -2;
    }
    
    // 初始化时关闭水泵
    ret = UM_GPIO_SetValue(PUMP_GPIO, UM_GPIO_LOW_LEVE);
    if (ret < 0) {
        printf("Failed to set initial pump GPIO value: %d\n", ret);
        return -3;
    }
    
    // printf("Pump control initialized successfully\n");
    return 0;
}

int pump_on(void)
{
    int ret = UM_GPIO_SetValue(PUMP_GPIO, UM_GPIO_HIGH_LEVE);
    if (ret < 0) {
        printf("Failed to turn on pump: %d\n", ret);
        return -1;
    }
    
    // printf("Pump turned ON\n");
    return 0;
}

int pump_off(void)
{
    int ret = UM_GPIO_SetValue(PUMP_GPIO, UM_GPIO_LOW_LEVE);
    if (ret < 0) {
        printf("Failed to turn off pump: %d\n", ret);
        return -1;
    }
    
    // printf("Pump turned OFF\n");
    return 0;
}

int pump_get_status(int *status)
{
    if (status == NULL) {
        printf("Invalid parameter: status is NULL\n");
        return -1;
    }
    
    int ret = UM_GPIO_GetValue(PUMP_GPIO, status);
    if (ret < 0) {
        printf("Failed to get pump status: %d\n", ret);
        return -2;
    }
    
    return 0;
}

int pump_deinit(void)
{
    // 关闭水泵
    UM_GPIO_SetValue(PUMP_GPIO, UM_GPIO_LOW_LEVE);
    
    // 取消导出GPIO
    int ret = UM_GPIO_Export(PUMP_GPIO, UM_GPIO_NOT_EXPORT);
    if (ret < 0) {
        printf("Failed to unexport pump GPIO: %d\n", ret);
        return -1;
    }
    
    // printf("Pump control released\n");
    return 0;
}
