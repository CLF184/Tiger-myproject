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
#include <iostream>
#include <string>
#include <algorithm>
#include <cstring>
#include <iterator>
#include <map>
#include <vector>
#include <fstream>
#include "serial_uart.h"
#include "myserial.h"

extern "C" {
#include <semaphore.h>
#include <pthread.h>
#include <sys/types.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <poll.h>
#include <errno.h>
}
using namespace std;

extern "C" void NotifyImageCapturedFromNative(const char* path);

#define UART_TTL_NAME "/dev/ttyS1"
#define MAX_BUFFER_SIZE 1024

#define FRAME_HEAD 0xFE //帧头
#define FRAME_END 0xFF  //帧尾
#define ESC        0x7E //转义
#define CAMERA_END 0x01 //相机

vector<unsigned char> data_buffer; //数据缓冲区
int frame_len = 0;
static int fd;
pthread_t pid_read;

static pthread_mutex_t g_uartInitMutex = PTHREAD_MUTEX_INITIALIZER;
static bool g_uartInited = false;

//int recv[MAX_BUFFER_SIZE];
pthread_mutex_t recv_mutex = PTHREAD_MUTEX_INITIALIZER;  // 初始化互斥锁

typedef struct {
    char* buf;  // 数据缓冲区
    int len;                   // 数据长度
} SerialOutputParams;

static uint8_t CalcChecksum(const vector<unsigned char>& data, size_t n)
{
    uint8_t sum = 0;
    for (size_t i = 0; i < n; ++i) {
        sum = static_cast<uint8_t>(sum + data[i]);
    }
    return sum;
}

void WriteFrameToFile(const vector<uint8_t>& frameData, const char* fileName)
{
    ofstream file(fileName, ios::binary | ios::trunc);
    if (!file) {
        cerr << "Failed to open file" << endl;
        return;
    }
    
    if (!file.write(reinterpret_cast<const char*>(frameData.data()), frameData.size())) {
        cerr << "Write failed" << endl;
    }
}

void *_serial_input_task(void* arg)// 串口读线程
{
    int count = 0;
    int status = 0;
    int ret = ERR;
    //int recv_temp[MAX_BUFFER_SIZE];
    unsigned char buf = 0;
    vector<unsigned char> data_buffer_temp;

    while (1) {
        ret = read(fd, &buf, 1);
        printf("%02X\n", buf);
        if (ret == ERR) {
            perror("read error");
            break;  // 读取失败，退出循环
        }
        if(status==0){ //等待帧头
            if(buf==FRAME_HEAD){
                status = 1; //切换-接收数据
                count = 0;
                data_buffer_temp.clear(); // 清空临时缓冲区
            }
        }
        else if(status==1){ //接收数据
            if (buf == ESC) {
                status=2;  //切换-处理转义
            }
            else if(buf==FRAME_END){ //结束接收
                status = 0; //切换-等待帧头
                if (!data_buffer_temp.empty()) {
                    const size_t payloadLen = data_buffer_temp.size() - 1;
                    const uint8_t recvChecksum = data_buffer_temp.back();
                    const uint8_t calcChecksum = CalcChecksum(data_buffer_temp, payloadLen);
                    if (recvChecksum == calcChecksum) {
                        frame_len = static_cast<int>(payloadLen);
                        pthread_mutex_lock(&recv_mutex);  // 加锁
                        data_buffer.assign(data_buffer_temp.begin(), data_buffer_temp.begin() + payloadLen); // 去除校验和字节
                        pthread_mutex_unlock(&recv_mutex);  // 解锁
                    }
                }
            }
            else if(buf==CAMERA_END){
                status = 0; //切换-等待帧头
                if (!data_buffer_temp.empty()) {
                    const size_t payloadLen = data_buffer_temp.size() - 1;
                    const uint8_t recvChecksum = data_buffer_temp.back();
                    const uint8_t calcChecksum = CalcChecksum(data_buffer_temp, payloadLen);
                    if (recvChecksum == calcChecksum) {
                        vector<uint8_t> imageData(data_buffer_temp.begin(), data_buffer_temp.begin() + payloadLen);
                        WriteFrameToFile(imageData,PHOTO_PATH);
                        cout<< "Frame received and written to file." <<endl;
                        NotifyImageCapturedFromNative(PHOTO_PATH);
                    }
                }
            }
            else {
                data_buffer_temp.push_back(buf); // 将数据存入临时缓冲区
                //recv_temp[count] = buf;
                count++;
            }
        }
        else if(status==2){ //处理转义
            data_buffer_temp.push_back(buf); // 将数据存入临时缓冲区
            //recv_temp[count] = buf;
            count++;
            status = 1; //切换-接收数据
        }
    }
    return NULL;
}

unsigned char* return_recv(int *len)
{
    *len = frame_len;
    unsigned char *temp; // 临时缓冲区
    
    temp = new unsigned char[data_buffer.size()]; // 动态分配内存

    pthread_mutex_lock(&recv_mutex);  // 加锁
    std::memcpy(temp, &data_buffer[0], data_buffer.size() * sizeof(data_buffer[0]));
    pthread_mutex_unlock(&recv_mutex);  // 解锁

    return temp;
}

// 串口写线程
void *_serial_output_task(void *arg)
{
    SerialOutputParams *params = (SerialOutputParams *)arg;  // 将参数转换为结构体指针
    const char* buf = params->buf;                 // 获取 buf
    int len = params->len;                                 // 获取 len

    write(fd, buf, len);

    delete[] params->buf;  // 释放动态分配的内存
    delete params;  // 释放动态分配的内存
    return NULL;
}


void write_uart(const char* buf, int len)
{
    pthread_t pid_write;

    // 动态分配结构体并填充参数
    SerialOutputParams *params = new SerialOutputParams;
    if (params == NULL) {
        perror("malloc failed");
        return;
    }
    
    params->buf = new char[len];
    std::memcpy(params->buf, buf, len);
    params->len = len;

    // 创建线程并传递参数
    pthread_create(&pid_write, NULL, _serial_output_task, (void *)params);
}

void init_uart(){
    pthread_mutex_lock(&g_uartInitMutex);
    if (g_uartInited) {
        pthread_mutex_unlock(&g_uartInitMutex);
        return;
    }

    int ret = ERR;

    fd = open(UART_TTL_NAME, O_RDWR | O_NONBLOCK);
    if (fd == ERR) {
        perror("open file fail\n");
        exit(-1);
    }
    ret = uart_init(fd, 115200L);
    if (ret == ERR) {
        perror("uart init error\n");
        exit(-1);
    }

    pthread_create(&pid_read, NULL, _serial_input_task, NULL);

    g_uartInited = true;
    pthread_mutex_unlock(&g_uartInitMutex);
}