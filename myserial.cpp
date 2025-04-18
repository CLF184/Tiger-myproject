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
}
using namespace std;

#define UART_TTL_NAME "/dev/ttyS1"
#define MAX_BUFFER_SIZE 1024

#define FRAME_HEAD 0xFE //帧头
#define FRAME_END 0xFF  //帧尾
#define ESC        0x7E //转义
#define CAMERA_END 0x01 //相机

#define PHOTO_PATH "/data/storage/el2/base/haps/entry/files/output.jpeg"

vector<unsigned char> data_buffer; //数据缓冲区
int frame_len = 0;
static int fd;
pthread_t pid_read;

//int recv[MAX_BUFFER_SIZE];
pthread_mutex_t recv_mutex = PTHREAD_MUTEX_INITIALIZER;  // 初始化互斥锁

typedef struct {
    char* buf;  // 数据缓冲区
    int len;                   // 数据长度
} SerialOutputParams;

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
        //printf("%02X\n", buf);
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
                //cout<<endl;
                frame_len = count;
                status = 0; //切换-等待帧头
                pthread_mutex_lock(&recv_mutex);  // 加锁
                data_buffer.assign(data_buffer_temp.begin(), data_buffer_temp.end()); // 将临时缓冲区的数据复制到数据缓冲区
                pthread_mutex_unlock(&recv_mutex);  // 解锁
            }
            else if(buf==CAMERA_END){
                //cout<<endl;
                status = 0; //切换-等待帧头
                //frame_len = count;
                WriteFrameToFile(data_buffer_temp,PHOTO_PATH);
                cout<< "Frame received and written to file." <<endl;
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
    memcpy(temp, &data_buffer[0], data_buffer.size() * sizeof(data_buffer[0]));
    pthread_mutex_unlock(&recv_mutex);  // 解锁

    return temp;
}

float get_data_by_key(char *key){
    int len = 0;
    unsigned char* data = return_recv(&len);
    
    if (data == NULL || len <= 0) {
        delete[] data;  // Free memory if data was allocated
        return 0.0f;    // Return default value if no data
    }

    // Convert to C-style string for parsing
    char dataStr[MAX_BUFFER_SIZE] = {0};
    memcpy(dataStr, data, (len < MAX_BUFFER_SIZE-1) ? len : MAX_BUFFER_SIZE-1);
    delete[] data;  // Free the allocated memory

    float value = 0.0f;
    char searchKey[32] = {0};
    sprintf(searchKey, "%s:", key);  // Format search key with colon

    char* position = strstr(dataStr, searchKey);
    if (position != NULL) {
        sscanf(position, "%*[^:]:%f", &value);
    }

    return value;
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
    memcpy(params->buf, buf, len);
    params->len = len;

    // 创建线程并传递参数
    pthread_create(&pid_write, NULL, _serial_output_task, (void *)params);
}

void init_uart(){
    int ret = ERR;

    fd = open(UART_TTL_NAME, O_RDWR);
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
}