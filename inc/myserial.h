/*
 * Copyright (c) 2022 Unionman Technology Co., Ltd.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef MYSERIAL_H
#define MYSERIAL_H

#include <stdint.h>

// 帧定义常量
#define UART_TTL_NAME "/dev/ttyS1"
#define MAX_BUFFER_SIZE 1024

#define FRAME_HEAD 0xFE  // 帧头
#define FRAME_END 0xFF   // 帧尾
#define ESC 0x7E         // 转义
#define CAMERA_END 0x01  // 相机


/**
 * @brief 初始化UART设备并启动读线程
 */
void init_uart();

/**
 * @brief 通过UART发送数据
 * 
 * @param buf 数据缓冲区
 * @param len 数据长度
 */
void write_uart(const char* buf, int len);

/**
 * @brief 返回接收到的数据
 * 
 * @param len 接收数据长度指针
 * @return 接收数据缓冲区
 * @note 调用者负责释放返回的内存
 */
unsigned char* return_recv(int* len);


/**
 * @brief 解析数据并获取指定键的值
 * 
 * @param key 键名
 * @return 键对应的值
 */
float get_data_by_key(char *key);

#endif // MYSERIAL_H
