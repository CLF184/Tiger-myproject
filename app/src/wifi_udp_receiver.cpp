/*
 * UDP 接收实现：
 * - 监听端口 9000
 * - 兼容两种上行数据：
 *   1) 纯文本 UDP：每个 datagram 视为一条文本记录，保存为最新内容
 *   2) 帧式二进制流（可跨 datagram“分包”）：
 *      帧头 0xFE，转义 0x7E，帧尾类型：
 *        - 0xFF：文本帧（传感器字符串），保存为最新内容
 *        - 0x01：相机帧（JPEG bytes），写入 PHOTO_PATH 并通知上层
 *
 * 说明：UDP 本身不会“拆包”应用层消息；你看到的 1460 字节分段通常来自发送端主动分块
 * 或 TCP 流式读取的分段行为。接收端需要把收到的字节当作连续流来解析，而不能按“包边界”解码。
 */

#include "wifi_udp_receiver.h"

#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include <algorithm>
#include <fstream>
#include <vector>

#include "myserial.h" // FRAME_HEAD/FRAME_END/ESC/CAMERA_END + PHOTO_PATH

extern "C" void NotifyImageCapturedFromNative(const char *path);

// 与串口通道保持一致的最大长度，避免 JSON 过长
static const size_t WIFI_UDP_BUF_SIZE = 1024;
static const size_t WIFI_MAX_FRAME_SIZE = 1024 * 1024; // 1MB，避免异常数据撑爆内存

static int g_udpSock = -1;
static pthread_t g_udpThread;
static pthread_mutex_t g_udpMutex = PTHREAD_MUTEX_INITIALIZER;
static bool g_udpInited = false;
static char g_udpData[WIFI_UDP_BUF_SIZE] = {0};

// 帧式解析状态：跨 UDP datagram 保持
static int g_frameStatus = 0; // 0=等待帧头, 1=接收数据, 2=转义中
static std::vector<uint8_t> g_frameBuf;

static void WriteBytesToFile(const std::vector<uint8_t> &bytes, const char *fileName)
{
    if (!fileName) {
        return;
    }
    std::ofstream file(fileName, std::ios::binary | std::ios::trunc);
    if (!file) {
        perror("open file failed");
        return;
    }
    if (!bytes.empty()) {
        file.write(reinterpret_cast<const char *>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    }
}

static void SaveLatestTextLocked(const uint8_t *buf, size_t n)
{
    // 假设 buf 为可打印文本（或至少无 '\0'），做截断保存
    size_t copyLen = std::min(n, WIFI_UDP_BUF_SIZE - 1);
    memcpy(g_udpData, buf, copyLen);
    g_udpData[copyLen] = '\0';
}

static void HandleCompleteFrame(uint8_t frameType)
{
    if (frameType == FRAME_END) {
        pthread_mutex_lock(&g_udpMutex);
        SaveLatestTextLocked(g_frameBuf.data(), g_frameBuf.size());
        pthread_mutex_unlock(&g_udpMutex);
    } else if (frameType == CAMERA_END) {
        WriteBytesToFile(g_frameBuf, PHOTO_PATH);
        NotifyImageCapturedFromNative(PHOTO_PATH);
    }
    g_frameBuf.clear();
}

static inline bool LooksLikeText(const uint8_t *buf, size_t n)
{
    if (!buf || n == 0) {
        return false;
    }
    // 简单判定：绝大多数是可打印 ASCII/常见分隔符；避免把二进制误当文本覆盖
    size_t printable = 0;
    for (size_t i = 0; i < n; i++) {
        const uint8_t c = buf[i];
        if (c == 0) {
            return false;
        }
        if (c == '\n' || c == '\r' || c == '\t' || (c >= 0x20 && c <= 0x7E)) {
            printable++;
        }
    }
    return printable * 100 / n >= 90; // 90% 以上可打印
}

static void ProcessIncomingBytes(const uint8_t *buf, size_t n)
{
    for (size_t i = 0; i < n; i++) {
        const uint8_t b = buf[i];

        if (g_frameStatus == 0) {
            // 等待帧头
            if (b == FRAME_HEAD) {
                g_frameStatus = 1;
                g_frameBuf.clear();
            }
            continue;
        }

        if (g_frameStatus == 2) {
            // 转义：原样收入
            g_frameBuf.push_back(b);
            g_frameStatus = 1;
        } else if (g_frameStatus == 1) {
            if (b == ESC) {
                g_frameStatus = 2;
            } else if (b == FRAME_HEAD) {
                // 重新同步：遇到新的帧头，丢弃旧的未完成帧
                g_frameBuf.clear();
                g_frameStatus = 1;
            } else if (b == FRAME_END || b == CAMERA_END) {
                HandleCompleteFrame(b);
                g_frameStatus = 0;
            } else {
                g_frameBuf.push_back(b);
            }
        }

        if (g_frameBuf.size() > WIFI_MAX_FRAME_SIZE) {
            // 防御：异常数据导致一直收不到帧尾
            g_frameBuf.clear();
            g_frameStatus = 0;
        }
    }
}

static void *udp_recv_task(void *arg)
{
    (void)arg;
    while (1) {
        // UDP datagram 最大可远超 1024，这里取 2KB 以容纳常见分块（如 1460）
        uint8_t buf[2048];
        ssize_t n = recvfrom(g_udpSock, buf, sizeof(buf), 0, nullptr, nullptr);
        if (n <= 0) {
            if (n < 0 && (errno == EINTR)) {
                continue;
            }
            // 其它错误简单打印后退出线程，避免复杂恢复逻辑
            perror("udp recvfrom error");
            break;
        }

        const size_t nn = static_cast<size_t>(n);

        // 1) 优先走“帧式流”解析（可跨 datagram 拼接）
        ProcessIncomingBytes(buf, nn);

        // 2) 兼容旧的“纯文本 UDP”模式：仅当当前不在帧式接收中时，才用该包覆盖最新文本
        if (g_frameStatus == 0 && LooksLikeText(buf, nn)) {
            pthread_mutex_lock(&g_udpMutex);
            SaveLatestTextLocked(buf, nn);
            pthread_mutex_unlock(&g_udpMutex);
        }
    }

    return nullptr;
}

void init_wifi_udp_receiver(void)
{
    pthread_mutex_lock(&g_udpMutex);
    if (g_udpInited) {
        pthread_mutex_unlock(&g_udpMutex);
        return;
    }

    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        perror("udp socket create fail");
        pthread_mutex_unlock(&g_udpMutex);
        return;
    }

    int reuse = 1;
    (void)setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY); // 接收任意地址的广播/单播
    addr.sin_port = htons(9000);              // 固定端口 9000

    if (bind(sock, reinterpret_cast<struct sockaddr *>(&addr), sizeof(addr)) < 0) {
        perror("udp bind fail");
        close(sock);
        pthread_mutex_unlock(&g_udpMutex);
        return;
    }

    g_udpSock = sock;

    if (pthread_create(&g_udpThread, nullptr, udp_recv_task, nullptr) != 0) {
        perror("udp pthread_create fail");
        close(sock);
        g_udpSock = -1;
        pthread_mutex_unlock(&g_udpMutex);
        return;
    }

    g_udpInited = true;
    pthread_mutex_unlock(&g_udpMutex);
}

int wifi_get_latest_data(char *outBuf, size_t bufLen)
{
    if (outBuf == nullptr || bufLen == 0) {
        return -1;
    }

    pthread_mutex_lock(&g_udpMutex);
    if (g_udpData[0] == '\0') {
        pthread_mutex_unlock(&g_udpMutex);
        return -1; // 还没有收到任何数据
    }

    size_t len = strnlen(g_udpData, WIFI_UDP_BUF_SIZE);
    if (len >= bufLen) {
        len = bufLen - 1;
    }
    memcpy(outBuf, g_udpData, len);
    outBuf[len] = '\0';
    pthread_mutex_unlock(&g_udpMutex);

    return 0;
}
