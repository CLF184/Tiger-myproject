#ifndef BUZZER_CONTROL_H
#define BUZZER_CONTROL_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 初始化蜂鸣器
 * 
 * @return int 成功返回0，失败返回负值
 */
int BuzzerInit(void);

/**
 * @brief 蜂鸣器鸣响
 * 
 * @param on 1-开启，0-关闭
 * @return int 成功返回0，失败返回负值
 */
int BuzzerControl(int on);

/**
 * @brief 蜂鸣器鸣响一段时间
 * 
 * @param milliseconds 持续时间(毫秒)
 * @return int 成功返回0，失败返回负值
 */
int BuzzerBeep(int milliseconds);

/**
 * @brief 释放蜂鸣器资源
 * 
 * @return int 成功返回0，失败返回负值
 */
int BuzzerDeinit(void);

#ifdef __cplusplus
}
#endif

#endif // BUZZER_CONTROL_H
