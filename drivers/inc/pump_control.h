#ifndef PUMP_CONTROL_H
#define PUMP_CONTROL_H

/**
 * @brief 初始化水泵控制
 * 
 * @return int 成功返回0，失败返回负值
 */
int pump_init(void);

/**
 * @brief 打开水泵
 * 
 * @return int 成功返回0，失败返回负值
 */
int pump_on(void);

/**
 * @brief 关闭水泵
 * 
 * @return int 成功返回0，失败返回负值
 */
int pump_off(void);

/**
 * @brief 获取水泵状态
 * 
 * @param status 输出参数，1表示开启，0表示关闭
 * @return int 成功返回0，失败返回负值
 */
int pump_get_status(int *status);

/**
 * @brief 释放水泵控制资源
 * 
 * @return int 成功返回0，失败返回负值
 */
int pump_deinit(void);

#endif /* PUMP_CONTROL_H */
