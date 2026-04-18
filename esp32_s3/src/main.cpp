#include <Adafruit_NeoPixel.h>
#include <WiFi.h>
#include <thread>
#include <vector>
#include <driver/uart.h>
#include <DHT.h>

#include "camera_module.h"
#include "dht_module.h"
#include "jw01_module.h"
#include "soil_module.h"
#include "udp_broadcast.h"

using namespace std;

#define PIN_PIXS 48
#define PIX_NUM 1

#define DHTPIN GPIO_NUM_2
#define DHTTYPE DHT11 // 定义传感器类型为DHT11

// 光照传感器 ADC 引脚（请根据实际接线修改）
#define LIGHT_SENSOR_PIN GPIO_NUM_19

#define RXD_PIN (GPIO_NUM_1)
#define UART_NUM UART_NUM_1

// RS485 土壤多参数传感器（按实际接线修改）
#define SOIL_RS485_RX_PIN (GPIO_NUM_42)
#define SOIL_RS485_TX_PIN (GPIO_NUM_41)
#define SOIL_RS485_DE_PIN (-1)
#define SOIL_RS485_BAUD 4800
#define SOIL_RS485_ADDR 0x01

#define FRAME_HEAD 0xFE
#define FRAME_END 0xFF
#define ESC 0x7E
#define CAMERA_END 0x01 //相机

// WiFi & UDP 配置（请根据实际情况修改）
const char *WIFI_SSID = "werrrrttt";
const char *WIFI_PASSWORD = "13719623327aa";
const uint16_t UDP_PORT = 9000;  // 传感器数据广播端口
const uint16_t UDP_CMD_PORT = 9001; // 命令接收端口

char *command;
QueueHandle_t cmdQueue = xQueueCreate(10, sizeof(command));

// Create a mutex for serial communication
SemaphoreHandle_t serialMutex = NULL;

Adafruit_NeoPixel pixels(PIX_NUM, PIN_PIXS, NEO_GRB + NEO_KHZ800);

static uint8_t CalcChecksum(const char *buf, int len)
{
  uint8_t sum = 0;
  for (int i = 0; i < len; i++) {
    sum = (uint8_t)(sum + (uint8_t)buf[i]);
  }
  return sum;
}

static inline void AppendEscapedByte(uint8_t *frame, int &pos, uint8_t value)
{
  if (value == ESC || value == FRAME_END || value == FRAME_HEAD || value == CAMERA_END) {
    frame[pos++] = ESC;
  }
  frame[pos++] = value;
}

// 保留原有帧格式(FRAME_HEAD/ESC/END)，并同时通过串口和 UDP 广播发送
void transmitData(const char *buf, int len,unsigned char type){
  // 帧格式：HEAD + escaped(payload + checksum) + type
  // 最坏情况 payload+checksum 每字节都需要转义：2*(len+1)，再加 HEAD 和 type
  const int maxLen = ((len + 1) * 2) + 2;
  uint8_t *frame = (uint8_t *)pvPortMalloc(maxLen);
  if (frame == NULL) {
    return;
  }
  int pos = 0;
  const uint8_t checksum = CalcChecksum(buf, len);

  if(xSemaphoreTake(serialMutex, portMAX_DELAY) == pdTRUE){
    // 帧头
    frame[pos++] = FRAME_HEAD;

    // 数据 + 转义
    for(int i = 0; i < len; i++) {
      uint8_t temp = (uint8_t)buf[i];
      AppendEscapedByte(frame, pos, temp);
    }

    // 校验和也参与转义
    AppendEscapedByte(frame, pos, checksum);

    // 结束/类型字节（例如 FRAME_END 或 CAMERA_END）
    frame[pos++] = (uint8_t)type;

    // 不做应用层分包：一次发出整个帧（UDP/IP 层可能会自动分片）
    udp_broadcast::send((const char *)frame, pos, type);
    Serial.printf("Transmitted frame: %d bytes (type: 0x%02X)\n", pos, type);
  }
  xSemaphoreGive(serialMutex);
  vPortFree(frame);
}

// void controlGPIO(const char* command) {
//   int pin;
//   char state[10];
//   sscanf(command, "GPIO %d %s", &pin, state);
//   //Serial.printf("Setting GPIO %d to %s\n", pin, state);
// }

// void controlPWM(const char* command) {
//   // 解析命令并控制PWM
//   // ...
//   int pin, value;
//   sscanf(command, "PWM %d %d", &pin, &value);
//   Serial.printf("Setting PWM on pin %d to %d\n", pin, value);
// }


// 命令-函数映射表
struct CommandHandler {
  const char* prefix;
  void (*handler)(const char*);
};

void send_sensor_data_once(const char *command);

const CommandHandler handlers[] = {
  // {"GPIO", controlGPIO},
  // {"PWM",  controlPWM},
  // {"ADC",  readADC},
  {"GET_DATA",send_sensor_data_once},
  {"CAPTURE",camera_module::capture_command},
  {"SETFRAMESIZE",camera_module::set_frame_size_command}
};

void showPixelColor(uint32_t c) {
  pixels.setPixelColor(0, c);
  pixels.show();
}


// 中断回调（仅超时触发）
void IRAM_ATTR onSerialData() {
  // 注意：该回调在中断上下文，避免使用 std::vector/new 等复杂操作
  static char rxBuf[256];
  int n = 0;
  while (Serial.available() && n < (int)sizeof(rxBuf) - 1) {
    rxBuf[n++] = (char)Serial.read();
  }
  rxBuf[n] = '\0';

  char *temp = (char *)pvPortMalloc(n + 1);
  if (temp == NULL) {
    return;
  }
  memcpy(temp, rxBuf, n + 1);
  xQueueSendFromISR(cmdQueue, &temp, NULL);
}

void processCommands() {
  char* receivedCmd;
  int cmd_len = 0;
  while(xQueueReceive(cmdQueue, &receivedCmd, 0) == pdTRUE){
    bool handled = false;
    for(auto& h : handlers) {
      if(strncmp(receivedCmd, h.prefix, strlen(h.prefix)) == 0) {
        h.handler(receivedCmd);
        handled = true;
        break;
      }
    }
    
    if(!handled) {
      //Serial.printf("ERROR: Unknown command '%s'\n", receivedCmd);
    }
    
    vPortFree(receivedCmd); // 释放内存
  }
}

void processCommandshandler(void *pvParameters) {
  while (true) {
    processCommands();
    vTaskDelay(1000 / portTICK_PERIOD_MS);
  }
}

// UDP 命令接收任务：监听 9001 端口并将命令送入 cmdQueue
void udpReceiveTask(void *pvParameters) {
  char buf[256];
  while (true) {
    int len = udp_broadcast::receive(buf, sizeof(buf) - 1);
    if (len > 0) {
      buf[len] = '\0';
      char *temp = (char *)pvPortMalloc(len + 1);
      if (temp != NULL) {
        memcpy(temp, buf, len + 1);
        xQueueSend(cmdQueue, &temp, portMAX_DELAY);
      }
    }
    vTaskDelay(1000 / portTICK_PERIOD_MS);
  }
}

void send_sensor_data_once(const char *command) {
  (void)command;
  dht_module::DhtData dht = {0};
  jw01_module::Jw01Data jw01 = {0};
  soil_module::SoilData soil = {0};
  char buffer[240];
  dht_module::read(dht);
  jw01_module::get(jw01);
  soil_module::get(soil);
  int rawLight = analogRead(LIGHT_SENSOR_PIN);
  float light = (rawLight / 4095.0f) * 100.0f; // 转换为百分比 0~100
  snprintf(buffer,
             sizeof(buffer),
             "Humi:%.3f;Temp:%.3f;CH2O:%.3f;TVOC:%.3f;CO_2:%.3f;SoilHumi:%.1f;SoilTemp:%.1f;EC:%.0f;pH:%.1f;N:%.0f;P:%.0f;K:%.0f;Salt:%.0f;TDS:%.0f;Light:%.1f;",
             dht.humidity,
             dht.temperature,
             jw01.ch2o,
             jw01.tvoc,
             jw01.co2,
             soil.moisture,
             soil.temperature,
             soil.ec,
             soil.ph,
             soil.n,
             soil.p,
             soil.k,
             soil.salt,
             soil.tds,
             light);
  transmitData(buffer, strlen(buffer), FRAME_END);
}

void setup() {
  serialMutex = xSemaphoreCreateMutex();
  Serial.begin(115200);
  pixels.begin();
  pixels.setBrightness(8);
  showPixelColor(0x0);

  // 初始化 WiFi + UDP 广播
  udp_broadcast::begin(WIFI_SSID, WIFI_PASSWORD, UDP_PORT);
  // 初始化 UDP 命令接收端口
  udp_broadcast::begin_rx(UDP_CMD_PORT);

  camera_module::set_transmit(transmitData);
  dht_module::init(DHTPIN, DHTTYPE);
  Serial.onReceive(onSerialData, true);
  jw01_module::init(Serial1, RXD_PIN);
  soil_module::init(Serial2,
                    SOIL_RS485_RX_PIN,
                    SOIL_RS485_TX_PIN,
                    SOIL_RS485_DE_PIN,
                    SOIL_RS485_BAUD,
                    SOIL_RS485_ADDR,
                    1000);
  xTaskCreate(processCommandshandler, "processCommandshandler", 4096, NULL, 1, NULL);
  xTaskCreate(udpReceiveTask, "udpReceiveTask", 4096, NULL, 1, NULL);
}

void loop() {
}