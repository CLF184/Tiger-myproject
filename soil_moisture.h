#ifndef _SOIL_MOISTURE_H_
#define _SOIL_MOISTURE_H_

#include "um_adc.h"  // 包含ADC功能的头文件

// 土壤湿度状态定义
#define SOIL_DRY      0  // 干燥
#define SOIL_MODERATE 1  // 适中
#define SOIL_WET      2  // 湿润

// 土壤湿度阈值定义（根据传感器特性调整）
#define DRY_THRESHOLD      1000  // 干燥阈值
#define WET_THRESHOLD      2500  // 湿润阈值

// 错误码定义
#define SOIL_MOISTURE_OK       0   // 操作成功
#define SOIL_MOISTURE_ERROR   -1   // 一般错误
#define SOIL_MOISTURE_ADC_ERR -2   // ADC读取错误

/**
 * 初始化土壤湿度传感器
 * 
 * @return SOIL_MOISTURE_OK 成功，其他值表示失败
 */
int soil_moisture_init(void);

/**
 * 读取土壤湿度原始ADC值
 * 
 * @param value 指向存储ADC值的整型变量的指针
 * @return SOIL_MOISTURE_OK 成功，其他值表示失败
 */
int soil_moisture_read_raw(int *value);

/**
 * 获取土壤湿度百分比
 * 
 * @param percentage 指向存储湿度百分比(0-100)的整型变量的指针
 * @return SOIL_MOISTURE_OK 成功，其他值表示失败
 */
int soil_moisture_read_percentage(int *percentage);

/**
 * 获取土壤湿度状态
 * 
 * @param status 指向存储湿度状态的整型变量的指针 (SOIL_DRY/SOIL_MODERATE/SOIL_WET)
 * @return SOIL_MOISTURE_OK 成功，其他值表示失败
 */
int soil_moisture_read_status(int *status);

#endif /* _SOIL_MOISTURE_H_ */