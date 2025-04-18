#ifndef LED_CONTROL_H
#define LED_CONTROL_H

#ifdef __cplusplus
extern "C" {
#endif

// LED控制状态码
#define LED_OK 0
#define LED_ERROR -1

// LED状态
#define LED_OFF 0
#define LED_ON 1

// 定义LED使用的GPIO引脚
#define LED_GPIO 381

/**
 * @brief 初始化LED
 * 
 * @return int 成功返回LED_OK，失败返回LED_ERROR
 */
int LedInit(void);

/**
 * @brief 打开LED
 * 
 * @return int 成功返回LED_OK，失败返回LED_ERROR
 */
int LedOn(void);

/**
 * @brief 关闭LED
 * 
 * @return int 成功返回LED_OK，失败返回LED_ERROR
 */
int LedOff(void);

/**
 * @brief 获取LED当前状态
 * 
 * @param status 用于存储LED状态的指针（LED_ON或LED_OFF）
 * @return int 成功返回LED_OK，失败返回LED_ERROR
 */
int LedGetStatus(int *status);

/**
 * @brief 翻转LED状态
 * 
 * @return int 成功返回LED_OK，失败返回LED_ERROR
 */
int LedToggle(void);

/**
 * @brief 释放LED资源
 * 
 * @return int 成功返回LED_OK，失败返回LED_ERROR
 */
int LedDeinit(void);

#ifdef __cplusplus
}
#endif

#endif /* LED_CONTROL_H */
