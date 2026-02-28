/*
 * 简单的 UDP 广播接收模块
 * 监听端口 9000，保存最近一帧文本数据，供 get_data_by_key 使用。
 */

#ifndef WIFI_UDP_RECEIVER_H
#define WIFI_UDP_RECEIVER_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 初始化 UDP 接收线程（监听 0.0.0.0:9000）
 */
void init_wifi_udp_receiver(void);

/**
 * @brief 取得最近一次收到的 UDP 文本数据
 *
 * @param outBuf 调用方提供的缓冲区
 * @param bufLen 缓冲区长度
 * @return 0 表示成功，有数据被拷贝；其他值表示当前还没有有效数据
 */
int wifi_get_latest_data(char *outBuf, size_t bufLen);

#ifdef __cplusplus
}
#endif

#endif // WIFI_UDP_RECEIVER_H
