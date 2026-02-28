#pragma once

#include <Arduino.h>
#include <WiFi.h>
#include <WiFiUdp.h>

namespace udp_broadcast {

// 初始化 WiFi 并准备 UDP 广播
// ssid / password: WiFi 名称和密码
// port: UDP 目标端口
void begin(const char *ssid, const char *password, uint16_t port);

// 发送一帧数据，通过 UDP 广播
// buf: 已按协议封装好的完整帧(FRAME_HEAD/ESC/END/type 等)
// len: 帧长度
// type: 预留参数，目前未在实现中使用
void send(const char *buf, int len, uint8_t type);

// 启动 UDP 接收（命令接收端口）
void begin_rx(uint16_t port);

// 轮询接收一帧命令数据
// 返回实际读取长度，<=0 表示当前无数据
int receive(char *buf, int maxLen);

} // namespace udp_broadcast
