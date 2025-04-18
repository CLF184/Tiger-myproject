#include "soil_moisture.h"
#include "um_adc.h"  // 包含ADC功能的头文件
#include <stdio.h>

// ADC通道选择
#define SOIL_MOISTURE_ADC_CHANNEL ADC_1

// ADC参考范围（根据实际硬件规格调整）
#define ADC_MIN_VALUE 0
#define ADC_MAX_VALUE 4095  // 12位ADC

/**
 * 初始化土壤湿度传感器
 */
int soil_moisture_init(void)
{
    // ADC初始化，根据提供的信息，不需要特别的初始化函数
    // printf("Soil moisture sensor initialized using ADC channel %d\n", SOIL_MOISTURE_ADC_CHANNEL);
    return SOIL_MOISTURE_OK;
}

/**
 * 读取土壤湿度原始ADC值
 */
int soil_moisture_read_raw(int *value)
{
    if (value == NULL) {
        return SOIL_MOISTURE_ERROR;
    }
    
    // 使用get_adc_data函数读取ADC值
    int ret = get_adc_data(SOIL_MOISTURE_ADC_CHANNEL, value);
    
    if (ret != ADC_OK) {
        printf("Error reading ADC: %d\n", ret);
        return SOIL_MOISTURE_ADC_ERR;
    }
    
    return SOIL_MOISTURE_OK;
}

/**
 * 获取土壤湿度百分比
 * 注意：这里我们假设ADC值越大，土壤越湿润
 * 根据实际传感器特性可能需要调整逻辑
 */
int soil_moisture_read_percentage(int *percentage)
{
    if (percentage == NULL) {
        return SOIL_MOISTURE_ERROR;
    }
    
    int raw_value;
    int ret = soil_moisture_read_raw(&raw_value);
    
    if (ret != SOIL_MOISTURE_OK) {
        return ret;
    }
    
    // 将ADC值映射到0-100%
    // 根据传感器特性，可能需要反转计算逻辑
    // 这里假设ADC值越大土壤越湿润
    *percentage = ((raw_value - ADC_MIN_VALUE) * 100) / (ADC_MAX_VALUE - ADC_MIN_VALUE);
    
    // 确保百分比在有效范围内
    if (*percentage < 0) {
        *percentage = 0;
    } else if (*percentage > 100) {
        *percentage = 100;
    }
    
    return SOIL_MOISTURE_OK;
}

/**
 * 获取土壤湿度状态
 */
int soil_moisture_read_status(int *status)
{
    if (status == NULL) {
        return SOIL_MOISTURE_ERROR;
    }
    
    int raw_value;
    int ret = soil_moisture_read_raw(&raw_value);
    
    if (ret != SOIL_MOISTURE_OK) {
        return ret;
    }
    
    // 根据阈值判断土壤湿度状态
    if (raw_value < DRY_THRESHOLD) {
        *status = SOIL_DRY;
    } else if (raw_value > WET_THRESHOLD) {
        *status = SOIL_WET;
    } else {
        *status = SOIL_MODERATE;
    }
    
    return SOIL_MOISTURE_OK;
}