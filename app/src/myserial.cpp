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
#include <vector>
#include <deque>
#include <fstream>
#include <cstring>

#include "serial_uart.h"
#include "myserial.h"

#include <pthread.h>
#include <sys/types.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <poll.h>
#include <errno.h>
#include <time.h>

using namespace std;

extern "C" void NotifyImageCapturedFromNative(const char* path);

// 协议号定义
static constexpr uint8_t PROTO_CMD = 0x10;     // 命令（需要 ACK）
static constexpr uint8_t PROTO_SENSOR = 0x11;  // 传感器数据（不需要 ACK）
static constexpr uint8_t PROTO_PHOTO = 0x12;   // 照片分包（需要 ACK）
static constexpr uint8_t PROTO_PHOTO_META = 0x13; // 照片元信息（需要 ACK）
static constexpr uint8_t PROTO_ACK = 0x7F;     // ACK

static constexpr int ACK_TIMEOUT_MS = 300;
static constexpr int MAX_RETRY_COUNT = 5;

static int g_fd = -1;
static pthread_t g_rxThread;
static pthread_t g_txThread;

// 传感器数据缓存（供 get_data_by_key 解析）
static vector<unsigned char> g_sensorBuf;
static int g_sensorLen = 0;
static pthread_mutex_t g_sensorMutex = PTHREAD_MUTEX_INITIALIZER;

// 发送队列（Stop-and-wait reliable sender）
struct TxJob {
    uint8_t proto;
    vector<uint8_t> payload;
    bool requireAck;
};

static deque<TxJob> g_txQueue;
static pthread_mutex_t g_txMutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t g_txCond = PTHREAD_COND_INITIALIZER;

static pthread_mutex_t g_sendMutex = PTHREAD_MUTEX_INITIALIZER;
static uint8_t g_txSeq = 0;

// ACK 等待状态
static pthread_mutex_t g_ackMutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t g_ackCond = PTHREAD_COND_INITIALIZER;
static bool g_waitingAck = false;
static uint8_t g_waitAckProto = 0;
static uint8_t g_waitAckSeq = 0;
static bool g_ackOk = false;

static inline bool ProtoRequiresAck(uint8_t proto)
{
    return proto == PROTO_CMD || proto == PROTO_PHOTO || proto == PROTO_PHOTO_META;
}

static uint16_t Crc16Ccitt(const uint8_t* data, size_t len)
{
    uint16_t crc = 0xFFFF;
    for (size_t i = 0; i < len; i++) {
        crc ^= static_cast<uint16_t>(data[i]) << 8;
        for (int b = 0; b < 8; b++) {
            crc = (crc & 0x8000) ? (crc << 1) ^ 0x1021 : (crc << 1);
        }
    }
    return crc;
}

static void AppendEscaped(vector<uint8_t>& out, uint8_t b)
{
    if (b == FRAME_HEAD || b == FRAME_END || b == ESC) {
        out.push_back(ESC);
    }
    out.push_back(b);
}

static vector<uint8_t> BuildFrame(uint8_t proto, uint8_t seq, const uint8_t* payload, uint16_t payloadLen)
{
    vector<uint8_t> raw;
    raw.reserve(1 + 1 + 2 + payloadLen + 2);
    raw.push_back(proto);
    raw.push_back(seq);
    raw.push_back(static_cast<uint8_t>(payloadLen & 0xFF));
    raw.push_back(static_cast<uint8_t>((payloadLen >> 8) & 0xFF));
    if (payloadLen > 0 && payload != nullptr) {
        raw.insert(raw.end(), payload, payload + payloadLen);
    }
    uint16_t crc = Crc16Ccitt(raw.data(), raw.size());
    raw.push_back(static_cast<uint8_t>(crc & 0xFF));
    raw.push_back(static_cast<uint8_t>((crc >> 8) & 0xFF));

    vector<uint8_t> framed;
    framed.reserve(raw.size() + 2);
    framed.push_back(FRAME_HEAD);
    for (uint8_t b : raw) {
        AppendEscaped(framed, b);
    }
    framed.push_back(FRAME_END);
    return framed;
}

static bool WriteAll(int fd, const uint8_t* buf, size_t len)
{
    size_t off = 0;
    while (off < len) {
        ssize_t n = write(fd, buf + off, len - off);
        if (n < 0) {
            if (errno == EINTR) {
                continue;
            }
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                usleep(1000);
                continue;
            }
            return false;
        }
        off += static_cast<size_t>(n);
    }
    return true;
}

static void WriteBytesToFile(const vector<uint8_t>& bytes, const char* fileName)
{
    ofstream file(fileName, ios::binary | ios::trunc);
    if (!file) {
        cerr << "Failed to open file" << endl;
        return;
    }
    if (!file.write(reinterpret_cast<const char*>(bytes.data()), bytes.size())) {
        cerr << "Write failed" << endl;
    }
}

static void SendAck(uint8_t ackProto, uint8_t ackSeq)
{
    uint8_t payload[2] = { ackProto, ackSeq };
    vector<uint8_t> frame = BuildFrame(PROTO_ACK, 0, payload, sizeof(payload));
    pthread_mutex_lock(&g_sendMutex);
    (void)WriteAll(g_fd, frame.data(), frame.size());
    pthread_mutex_unlock(&g_sendMutex);
}

static bool WaitAck(uint8_t proto, uint8_t seq, int timeoutMs)
{
    pthread_mutex_lock(&g_ackMutex);
    g_waitingAck = true;
    g_waitAckProto = proto;
    g_waitAckSeq = seq;
    g_ackOk = false;

    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    ts.tv_sec += timeoutMs / 1000;
    ts.tv_nsec += static_cast<long>(timeoutMs % 1000) * 1000000L;
    if (ts.tv_nsec >= 1000000000L) {
        ts.tv_sec += 1;
        ts.tv_nsec -= 1000000000L;
    }

    while (!g_ackOk) {
        int rc = pthread_cond_timedwait(&g_ackCond, &g_ackMutex, &ts);
        if (rc == ETIMEDOUT) {
            break;
        }
    }

    bool ok = g_ackOk;
    g_waitingAck = false;
    pthread_mutex_unlock(&g_ackMutex);
    return ok;
}

static void* TxWorker(void*)
{
    while (true) {
        TxJob job;
        pthread_mutex_lock(&g_txMutex);
        while (g_txQueue.empty()) {
            pthread_cond_wait(&g_txCond, &g_txMutex);
        }
        job = std::move(g_txQueue.front());
        g_txQueue.pop_front();
        pthread_mutex_unlock(&g_txMutex);

        uint8_t seq;
        pthread_mutex_lock(&g_sendMutex);
        seq = ++g_txSeq;
        pthread_mutex_unlock(&g_sendMutex);

        int retries = 0;
        while (true) {
            vector<uint8_t> frame = BuildFrame(job.proto, seq, job.payload.data(), static_cast<uint16_t>(job.payload.size()));
            pthread_mutex_lock(&g_sendMutex);
            bool okWrite = WriteAll(g_fd, frame.data(), frame.size());
            pthread_mutex_unlock(&g_sendMutex);

            if (!job.requireAck) {
                break;
            }

            bool okAck = okWrite && WaitAck(job.proto, seq, ACK_TIMEOUT_MS);
            if (okAck) {
                break;
            }

            retries++;
            if (retries > MAX_RETRY_COUNT) {
                break;
            }
        }
    }
    return nullptr;
}

static bool HandleFrame(const vector<uint8_t>& raw)
{
    // raw = [proto, seq, lenLo, lenHi, payload..., crcLo, crcHi]
    if (raw.size() < 1 + 1 + 2 + 2) {
        return false;
    }
    uint8_t proto = raw[0];
    uint8_t seq = raw[1];
    uint16_t len = static_cast<uint16_t>(raw[2]) | (static_cast<uint16_t>(raw[3]) << 8);
    if (raw.size() != static_cast<size_t>(1 + 1 + 2 + len + 2)) {
        return false;
    }
    const uint8_t* payload = (len > 0) ? &raw[4] : nullptr;
    uint16_t rxCrc = static_cast<uint16_t>(raw[4 + len]) | (static_cast<uint16_t>(raw[4 + len + 1]) << 8);
    uint16_t calc = Crc16Ccitt(raw.data(), 4 + len);
    if (rxCrc != calc) {
        return false;
    }

    if (proto == PROTO_ACK) {
        if (len < 2) {
            return true;
        }
        uint8_t ackProto = payload[0];
        uint8_t ackSeq = payload[1];

        pthread_mutex_lock(&g_ackMutex);
        if (g_waitingAck && ackProto == g_waitAckProto && ackSeq == g_waitAckSeq) {
            g_ackOk = true;
            pthread_cond_broadcast(&g_ackCond);
        }
        pthread_mutex_unlock(&g_ackMutex);
        return true;
    }

    if (ProtoRequiresAck(proto)) {
        SendAck(proto, seq);
    }

    // 传感器数据：直接覆盖缓存
    if (proto == PROTO_SENSOR) {
        pthread_mutex_lock(&g_sensorMutex);
        g_sensorBuf.assign(payload, payload + len);
        g_sensorLen = static_cast<int>(len);
        pthread_mutex_unlock(&g_sensorMutex);
        return true;
    }

    // 照片分包重组（transferId 用于将多帧归并为同一张照片；seq 仅用于 ACK 匹配）
    static uint8_t photoTransferId = 0;
    static bool photoActive = false;
    static uint16_t photoTotal = 0;
    static uint32_t photoTotalLen = 0;
    static uint16_t photoNextIndex = 0;
    static vector<uint8_t> photoBytes;

    if (proto == PROTO_PHOTO_META) {
        // payload: transferId(1) + totalChunks(u16) + totalLen(u32)
        if (len < 1 + 2 + 4) {
            return true;
        }
        uint8_t tid = payload[0];
        uint16_t total = static_cast<uint16_t>(payload[1]) | (static_cast<uint16_t>(payload[2]) << 8);
        uint32_t totalLen = static_cast<uint32_t>(payload[3]) |
                            (static_cast<uint32_t>(payload[4]) << 8) |
                            (static_cast<uint32_t>(payload[5]) << 16) |
                            (static_cast<uint32_t>(payload[6]) << 24);

        photoTransferId = tid;
        photoActive = true;
        photoTotal = total;
        photoTotalLen = totalLen;
        photoNextIndex = 0;
        photoBytes.clear();
        if (photoTotalLen > 0) {
            photoBytes.reserve(photoTotalLen);
        }
        return true;
    }

    if (proto == PROTO_PHOTO) {
        // payload: transferId(1) + idx(u16) + totalChunks(u16) + chunk...
        if (len < 1 + 2 + 2) {
            return true;
        }
        uint8_t tid = payload[0];
        uint16_t idx = static_cast<uint16_t>(payload[1]) | (static_cast<uint16_t>(payload[2]) << 8);
        uint16_t total = static_cast<uint16_t>(payload[3]) | (static_cast<uint16_t>(payload[4]) << 8);
        const uint8_t* chunk = payload + 5;
        uint16_t chunkLen = static_cast<uint16_t>(len - 5);

        if (!photoActive || tid != photoTransferId) {
            // 如果未收到 meta，也允许直接以首包启动（但没有总长度预分配）
            photoTransferId = tid;
            photoActive = true;
            photoTotal = total;
            photoTotalLen = 0;
            photoNextIndex = 0;
            photoBytes.clear();
        }

        if (photoTotal == 0) {
            photoTotal = total;
        }

        if (total == 0 || idx >= total) {
            return true;
        }
        if (idx < photoNextIndex) {
            return true; // 重复包（重传）
        }
        if (idx != photoNextIndex) {
            return true; // 非预期乱序（stop-and-wait 下不应出现）
        }

        photoBytes.insert(photoBytes.end(), chunk, chunk + chunkLen);
        photoNextIndex++;

        if (photoNextIndex == total) {
            WriteBytesToFile(photoBytes, PHOTO_PATH);
            NotifyImageCapturedFromNative(PHOTO_PATH);
            photoActive = false;
        }
        return true;
    }

    return true;
}

static void* RxWorker(void*)
{
    int state = 0; // 0: wait head, 1: in frame, 2: escape
    vector<uint8_t> raw;

    struct pollfd pfd;
    pfd.fd = g_fd;
    pfd.events = POLLIN;
    pfd.revents = 0;

    while (true) {
        pfd.revents = 0;
        int poll_ret = poll(&pfd, 1, 1000);
        if (poll_ret == 0) {
            continue;
        }
        if (poll_ret < 0) {
            if (errno == EINTR) {
                continue;
            }
            break;
        }
        if (pfd.revents & (POLLERR | POLLHUP | POLLNVAL)) {
            break;
        }
        if (!(pfd.revents & POLLIN)) {
            continue;
        }

        uint8_t b = 0;
        ssize_t n = read(g_fd, &b, 1);
        if (n == 0) {
            break;
        }
        if (n < 0) {
            if (errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK) {
                continue;
            }
            break;
        }

        if (state == 0) {
            if (b == FRAME_HEAD) {
                raw.clear();
                state = 1;
            }
            continue;
        }
        if (state == 2) {
            raw.push_back(b);
            state = 1;
            continue;
        }

        // state == 1
        if (b == ESC) {
            state = 2;
            continue;
        }
        if (b == FRAME_HEAD) {
            raw.clear();
            state = 1;
            continue;
        }
        if (b == FRAME_END) {
            (void)HandleFrame(raw);
            raw.clear();
            state = 0;
            continue;
        }
        raw.push_back(b);
    }
    return nullptr;
}

unsigned char* return_recv(int* len)
{
    if (len == nullptr) {
        return nullptr;
    }

    pthread_mutex_lock(&g_sensorMutex);
    int localLen = g_sensorLen;
    size_t sz = g_sensorBuf.size();
    pthread_mutex_unlock(&g_sensorMutex);

    *len = localLen;
    if (sz == 0 || localLen <= 0) {
        return nullptr;
    }

    unsigned char* temp = new unsigned char[sz];
    pthread_mutex_lock(&g_sensorMutex);
    memcpy(temp, g_sensorBuf.data(), sz);
    pthread_mutex_unlock(&g_sensorMutex);
    return temp;
}

float get_data_by_key(char* key)
{
    int len = 0;
    unsigned char* data = return_recv(&len);
    if (data == nullptr || len <= 0) {
        delete[] data;
        return 0.0f;
    }

    char dataStr[MAX_BUFFER_SIZE] = {0};
    memcpy(dataStr, data, (len < MAX_BUFFER_SIZE - 1) ? len : MAX_BUFFER_SIZE - 1);
    delete[] data;

    float value = 0.0f;
    char searchKey[32] = {0};
    sprintf(searchKey, "%s:", key);

    char* position = strstr(dataStr, searchKey);
    if (position != nullptr) {
        sscanf(position, "%*[^:]:%f", &value);
    }
    return value;
}

void write_uart(const char* buf, int len)
{
    if (buf == nullptr || len <= 0) {
        return;
    }
    TxJob job;
    job.proto = PROTO_CMD;
    job.requireAck = true;
    job.payload.assign(reinterpret_cast<const uint8_t*>(buf), reinterpret_cast<const uint8_t*>(buf) + len);

    pthread_mutex_lock(&g_txMutex);
    g_txQueue.push_back(std::move(job));
    pthread_cond_signal(&g_txCond);
    pthread_mutex_unlock(&g_txMutex);
}

void init_uart()
{
    int ret = ERR;
    g_fd = open(UART_TTL_NAME, O_RDWR | O_NONBLOCK);
    if (g_fd == ERR) {
        perror("open file fail\n");
        exit(-1);
    }

    ret = uart_init(g_fd, 115200L);
    if (ret == ERR) {
        perror("uart init error\n");
        exit(-1);
    }

    pthread_create(&g_rxThread, nullptr, RxWorker, nullptr);
    pthread_create(&g_txThread, nullptr, TxWorker, nullptr);
}