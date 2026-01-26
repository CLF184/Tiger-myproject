#ifndef LIGHT_SENSOR_H
#define LIGHT_SENSOR_H

/**
 * @brief 初始化光敏电阻ADC
 * 
 * @return int 成功返回0，失败返回-1
 */
int light_sensor_init(void);

/**
 * @brief 读取光敏电阻的值
 * 
 * @param value 指向存储读取值的整型变量的指针
 * @return int 成功返回0，失败返回-1
 */
int light_sensor_read(int *value);

/**
 * @brief 将ADC数值转换为光照强度百分比
 * 
 * @param adc_value ADC读取值
 * @return float 光照强度百分比(0-100)
 */
float light_sensor_to_percentage(int adc_value);

#endif /* LIGHT_SENSOR_H */