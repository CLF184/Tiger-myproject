#include "udp_broadcast.h"

namespace udp_broadcast {

static WiFiUDP udp;
static uint16_t g_port = 0;
static IPAddress g_broadcastIp(255, 255, 255, 255); // 简单起见，直接使用全局广播

static WiFiUDP udp_rx;
static uint16_t g_rx_port = 0;

void begin(const char *ssid, const char *password, uint16_t port) {
  g_port = port;

  if (WiFi.status() != WL_CONNECTED) {
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, password);

    // 简单等待连接，不做复杂状态机
    unsigned long start = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - start < 15000) {
      delay(500);
    }
  }

  // 启动本地 UDP（端口可以和目标端口相同，简单处理）
  udp.begin(g_port);
}

void send(const char *buf, int len, uint8_t type) {
  if (WiFi.status() != WL_CONNECTED) {
    return; // 未连接 WiFi 时直接返回
  }

  if (g_port == 0) {
    return; // 未初始化
  }

  udp.beginPacket(g_broadcastIp, g_port);
  // 现在由调用者构造完整帧(包含 FRAME_HEAD/ESC/END/type)，这里直接发送缓冲区
  udp.write(reinterpret_cast<const uint8_t *>(buf), len);
  udp.endPacket();
}

void begin_rx(uint16_t port) {
  g_rx_port = port;
  udp_rx.begin(g_rx_port);
}

int receive(char *buf, int maxLen) {
  if (g_rx_port == 0) {
    return 0;
  }

  int packetSize = udp_rx.parsePacket();
  if (packetSize <= 0) {
    return 0;
  }

  if (packetSize > maxLen) {
    packetSize = maxLen;
  }

  int len = udp_rx.read(reinterpret_cast<uint8_t *>(buf), packetSize);
  return len;
}

} // namespace udp_broadcast
