#include "esp_camera.h"
#include <Adafruit_NeoPixel.h>
#include <WiFi.h>
#include <thread>
#include <string>
#include <vector>
#include <cstring>
#include <driver/uart.h>
#include <DHT.h>

using namespace std;

#define CAMERA_MODEL_ESP32S3_EYE

#include "camera_pins.h"

#define PIN_PIXS 48
#define PIX_NUM 1

#define DHTPIN 2
#define DHTTYPE DHT11 // 定义传感器类型为DHT11

DHT dht(DHTPIN, DHTTYPE); // 创建DHT传感器对象

#define RXD_PIN (GPIO_NUM_1)
#define UART_NUM UART_NUM_1

#define FRAME_HEAD 0xFE
#define FRAME_END 0xFF
#define ESC 0x7E

// 协议号定义（与 OHOS 侧一致）
static constexpr uint8_t PROTO_CMD = 0x10;     // 命令（需要 ACK）
static constexpr uint8_t PROTO_SENSOR = 0x11;  // 传感器数据（不需要 ACK）
static constexpr uint8_t PROTO_PHOTO = 0x12;   // 照片分包（需要 ACK）
static constexpr uint8_t PROTO_PHOTO_META = 0x13; // 照片元信息（需要 ACK）
static constexpr uint8_t PROTO_ACK = 0x7F;     // ACK

static constexpr uint32_t ACK_TIMEOUT_MS = 300;
static constexpr int MAX_RETRY_COUNT = 5;
static constexpr uint16_t PHOTO_CHUNK_BYTES = 512;

struct FrameMsg {
  uint8_t proto;
  uint8_t seq;
  uint16_t len;
  uint8_t* payload;
};

static QueueHandle_t frameQueue = NULL;

// Create a mutex for serial communication
SemaphoreHandle_t serialMutex = NULL;

// Reliable sender (stop-and-wait)
static SemaphoreHandle_t reliableMutex = NULL;
static SemaphoreHandle_t ackSem = NULL;
static volatile uint8_t g_lastAckProto = 0;
static volatile uint8_t g_lastAckSeq = 0;
static uint8_t g_txSeq = 0;

portMUX_TYPE jw01_spinlock = portMUX_INITIALIZER_UNLOCKED;

Adafruit_NeoPixel pixels(PIX_NUM, PIN_PIXS, NEO_GRB + NEO_KHZ800);

float CH2O = 0;
float TVOC = 0;
float CO2 = 0;

static inline bool ProtoRequiresAck(uint8_t proto)
{
  return proto == PROTO_CMD || proto == PROTO_PHOTO || proto == PROTO_PHOTO_META;
}

static uint16_t Crc16Ccitt(const uint8_t* data, size_t len)
{
  uint16_t crc = 0xFFFF;
  for (size_t i = 0; i < len; i++) {
    crc ^= (uint16_t)data[i] << 8;
    for (int b = 0; b < 8; b++) {
      crc = (crc & 0x8000) ? (crc << 1) ^ 0x1021 : (crc << 1);
    }
  }
  return crc;
}

static void AppendEscaped(std::vector<uint8_t>& out, uint8_t b)
{
  if (b == FRAME_HEAD || b == FRAME_END || b == ESC) {
    out.push_back(ESC);
  }
  out.push_back(b);
}

static std::vector<uint8_t> BuildFrame(uint8_t proto, uint8_t seq, const uint8_t* payload, uint16_t payloadLen)
{
  std::vector<uint8_t> raw;
  raw.reserve(1 + 1 + 2 + payloadLen + 2);
  raw.push_back(proto);
  raw.push_back(seq);
  raw.push_back((uint8_t)(payloadLen & 0xFF));
  raw.push_back((uint8_t)((payloadLen >> 8) & 0xFF));
  if (payloadLen > 0 && payload != nullptr) {
    raw.insert(raw.end(), payload, payload + payloadLen);
  }
  uint16_t crc = Crc16Ccitt(raw.data(), raw.size());
  raw.push_back((uint8_t)(crc & 0xFF));
  raw.push_back((uint8_t)((crc >> 8) & 0xFF));

  std::vector<uint8_t> framed;
  framed.reserve(raw.size() + 2);
  framed.push_back(FRAME_HEAD);
  for (uint8_t b : raw) {
    AppendEscaped(framed, b);
  }
  framed.push_back(FRAME_END);
  return framed;
}

static void SerialWriteFrame(const std::vector<uint8_t>& frame)
{
  if (xSemaphoreTake(serialMutex, portMAX_DELAY) == pdTRUE) {
    Serial.write(frame.data(), frame.size());
    xSemaphoreGive(serialMutex);
  }
}

static void SendAck(uint8_t ackProto, uint8_t ackSeq)
{
  uint8_t payload[2] = { ackProto, ackSeq };
  auto frame = BuildFrame(PROTO_ACK, 0, payload, sizeof(payload));
  SerialWriteFrame(frame);
}

static bool SendFrameUnreliable(uint8_t proto, const uint8_t* payload, uint16_t len)
{
  uint8_t seq = ++g_txSeq;
  auto frame = BuildFrame(proto, seq, payload, len);
  SerialWriteFrame(frame);
  return true;
}

static bool SendFrameReliable(uint8_t proto, const uint8_t* payload, uint16_t len)
{
  if (xSemaphoreTake(reliableMutex, portMAX_DELAY) != pdTRUE) {
    return false;
  }

  // 清掉历史 ACK 信号
  while (xSemaphoreTake(ackSem, 0) == pdTRUE) {
  }

  uint8_t seq = ++g_txSeq;
  int retries = 0;
  while (true) {
    auto frame = BuildFrame(proto, seq, payload, len);
    SerialWriteFrame(frame);

    // 等待 ACK
    TickType_t deadline = xTaskGetTickCount() + pdMS_TO_TICKS(ACK_TIMEOUT_MS);
    bool got = false;
    while (xTaskGetTickCount() < deadline) {
      TickType_t remain = deadline - xTaskGetTickCount();
      if (xSemaphoreTake(ackSem, remain) == pdTRUE) {
        if (g_lastAckProto == proto && g_lastAckSeq == seq) {
          got = true;
          break;
        }
      }
    }

    if (got) {
      xSemaphoreGive(reliableMutex);
      return true;
    }

    retries++;
    if (retries > MAX_RETRY_COUNT) {
      xSemaphoreGive(reliableMutex);
      return false;
    }
  }
}

void sendPhoto(void *pvParameters){
  camera_fb_t *fb = esp_camera_fb_get();
  if (!fb) {
    vTaskDelete(NULL);
    return;
  }

  uint32_t totalChunks = (fb->len + PHOTO_CHUNK_BYTES - 1) / PHOTO_CHUNK_BYTES;
  if (totalChunks == 0) {
    esp_camera_fb_return(fb);
    vTaskDelete(NULL);
    return;
  }

  static uint8_t photoTransferId = 0;
  uint8_t tid = ++photoTransferId;

  // 先发送照片元信息：transferId + 总包数 + 总长度
  {
    std::vector<uint8_t> meta;
    meta.reserve(1 + 2 + 4);
    uint16_t total16 = (uint16_t)totalChunks;
    uint32_t totalLen = (uint32_t)fb->len;
    meta.push_back(tid);
    meta.push_back((uint8_t)(total16 & 0xFF));
    meta.push_back((uint8_t)((total16 >> 8) & 0xFF));
    meta.push_back((uint8_t)(totalLen & 0xFF));
    meta.push_back((uint8_t)((totalLen >> 8) & 0xFF));
    meta.push_back((uint8_t)((totalLen >> 16) & 0xFF));
    meta.push_back((uint8_t)((totalLen >> 24) & 0xFF));

    bool okMeta = SendFrameReliable(PROTO_PHOTO_META, meta.data(), (uint16_t)meta.size());
    if (!okMeta) {
      esp_camera_fb_return(fb);
      vTaskDelete(NULL);
      return;
    }
  }

  for (uint32_t idx = 0; idx < totalChunks; idx++) {
    uint32_t off = idx * PHOTO_CHUNK_BYTES;
    uint32_t chunkLen = fb->len - off;
    if (chunkLen > PHOTO_CHUNK_BYTES) {
      chunkLen = PHOTO_CHUNK_BYTES;
    }

    std::vector<uint8_t> payload;
    payload.reserve(1 + 4 + chunkLen);
    uint16_t idx16 = (uint16_t)idx;
    uint16_t total16 = (uint16_t)totalChunks;
    payload.push_back(tid);
    payload.push_back((uint8_t)(idx16 & 0xFF));
    payload.push_back((uint8_t)((idx16 >> 8) & 0xFF));
    payload.push_back((uint8_t)(total16 & 0xFF));
    payload.push_back((uint8_t)((total16 >> 8) & 0xFF));
    payload.insert(payload.end(), fb->buf + off, fb->buf + off + chunkLen);

    bool ok = SendFrameReliable(PROTO_PHOTO, payload.data(), (uint16_t)payload.size());
    if (!ok) {
      break;
    }
  }

  esp_camera_fb_return(fb);
  vTaskDelete(NULL);
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

void capture(const char* command) {
  xTaskCreate(sendPhoto, "sendPhoto",32768, NULL, 1, NULL);
}

void setframeSize(const char* command) {
  sensor_t *s = esp_camera_sensor_get();
  if (!s) {
    //Serial.println("Failed to get camera sensor");
    return;
  }

  framesize_t frameSize;
  if (strcmp(command, "SETFRAMESIZE 96X96") == 0) {
    frameSize = FRAMESIZE_96X96;
  } else if (strcmp(command, "SETFRAMESIZE QQVGA") == 0) {
    frameSize = FRAMESIZE_QQVGA;
  } else if (strcmp(command, "SETFRAMESIZE QCIF") == 0) {
    frameSize = FRAMESIZE_QCIF;
  } else if (strcmp(command, "SETFRAMESIZE HQVGA") == 0) {
    frameSize = FRAMESIZE_HQVGA;
  } else if (strcmp(command, "SETFRAMESIZE 240X240") == 0) {
    frameSize = FRAMESIZE_240X240;
  } else if (strcmp(command, "SETFRAMESIZE QVGA") == 0) {
    frameSize = FRAMESIZE_QVGA;
  } else if (strcmp(command, "SETFRAMESIZE CIF") == 0) {
    frameSize = FRAMESIZE_CIF;
  } else if (strcmp(command, "SETFRAMESIZE HVGA") == 0) {
    frameSize = FRAMESIZE_HVGA;
  } else if (strcmp(command, "SETFRAMESIZE VGA") == 0) {
    frameSize = FRAMESIZE_VGA;
  } else if (strcmp(command, "SETFRAMESIZE SVGA") == 0) {
    frameSize = FRAMESIZE_SVGA;
  } else if (strcmp(command, "SETFRAMESIZE XGA") == 0) {
    frameSize = FRAMESIZE_XGA;
  } else if (strcmp(command, "SETFRAMESIZE HD") == 0) {
    frameSize = FRAMESIZE_HD;
  } else if (strcmp(command, "SETFRAMESIZE SXGA") == 0) {
    frameSize = FRAMESIZE_SXGA;
  } else if (strcmp(command, "SETFRAMESIZE UXGA") == 0) {
    frameSize = FRAMESIZE_UXGA;
  } else {
    //Serial.println("Invalid frame size");
    return;
  }

  s->set_framesize(s, frameSize);
  Serial.printf("Frame size set to %s\n", command + 12);
}

// 命令-函数映射表
struct CommandHandler {
  const char* prefix;
  void (*handler)(const char*);
};

const CommandHandler handlers[] = {
  // {"GPIO", controlGPIO},
  // {"PWM",  controlPWM},
  // {"ADC",  readADC},
  {"CAPTURE",capture},
  {"SETFRAMESIZE",setframeSize}
};

void showPixelColor(uint32_t c) {
  pixels.setPixelColor(0, c);
  pixels.show();
}

void inline initCamera() {
  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sccb_sda = SIOD_GPIO_NUM;
  config.pin_sccb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.frame_size = FRAMESIZE_VGA;
  config.pixel_format = PIXFORMAT_JPEG;
  config.grab_mode = CAMERA_GRAB_WHEN_EMPTY;
  config.fb_location = CAMERA_FB_IN_PSRAM;
  config.jpeg_quality = 12;
  config.fb_count = 1;
  if (psramFound()) {
    //Serial.printf("PS RAM Found [%d]\n", ESP.getPsramSize());
    config.jpeg_quality = 10;
    config.fb_count = 2;
    config.grab_mode = CAMERA_GRAB_LATEST;
  } else {
    config.frame_size = FRAMESIZE_SVGA;
    config.fb_location = CAMERA_FB_IN_DRAM;
  }
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed with error 0x%x", err);
    return;
  }
  sensor_t *s = esp_camera_sensor_get();
  //s->set_vflip(s, 1);      // flip it back
  s->set_brightness(s, 1); // up the brightness just a bit
  s->set_saturation(s, 0); // lower the saturation
}

static void HandleFrameMsg(FrameMsg* msg)
{
  if (!msg) return;

  if (msg->proto == PROTO_ACK) {
    if (msg->len >= 2) {
      g_lastAckProto = msg->payload[0];
      g_lastAckSeq = msg->payload[1];
      xSemaphoreGive(ackSem);
    }
    return;
  }

  if (ProtoRequiresAck(msg->proto)) {
    SendAck(msg->proto, msg->seq);
  }

  if (msg->proto == PROTO_CMD) {
    // payload 是命令字符串（不带 '\0'）
    std::string cmd((const char*)msg->payload, (size_t)msg->len);
    bool handled = false;
    for (auto& h : handlers) {
      if (strncmp(cmd.c_str(), h.prefix, strlen(h.prefix)) == 0) {
        h.handler(cmd.c_str());
        handled = true;
        break;
      }
    }
    (void)handled;
  }
}

static void frameHandlerTask(void *pvParameters)
{
  while (true) {
    FrameMsg* msg = nullptr;
    if (xQueueReceive(frameQueue, &msg, portMAX_DELAY) == pdTRUE) {
      HandleFrameMsg(msg);
      if (msg) {
        free(msg->payload);
        delete msg;
      }
    }
  }
}

static bool ParseRawToMsg(const std::vector<uint8_t>& raw, FrameMsg*& out)
{
  out = nullptr;
  if (raw.size() < 1 + 1 + 2 + 2) {
    return false;
  }
  uint8_t proto = raw[0];
  uint8_t seq = raw[1];
  uint16_t len = (uint16_t)raw[2] | ((uint16_t)raw[3] << 8);
  if (raw.size() != (size_t)(1 + 1 + 2 + len + 2)) {
    return false;
  }
  uint16_t rxCrc = (uint16_t)raw[4 + len] | ((uint16_t)raw[4 + len + 1] << 8);
  uint16_t calc = Crc16Ccitt(raw.data(), 4 + len);
  if (rxCrc != calc) {
    return false;
  }

  FrameMsg* msg = new FrameMsg();
  msg->proto = proto;
  msg->seq = seq;
  msg->len = len;
  msg->payload = nullptr;
  if (len > 0) {
    msg->payload = (uint8_t*)malloc(len);
    if (!msg->payload) {
      delete msg;
      return false;
    }
    memcpy(msg->payload, &raw[4], len);
  }
  out = msg;
  return true;
}

static void serialRxTask(void *pvParameters)
{
  int state = 0; // 0: wait head, 1: in frame, 2: escape
  std::vector<uint8_t> raw;
  raw.reserve(1024);

  while (true) {
    while (Serial.available()) {
      uint8_t b = (uint8_t)Serial.read();
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
        FrameMsg* msg = nullptr;
        if (ParseRawToMsg(raw, msg) && msg != nullptr) {
          xQueueSend(frameQueue, &msg, 0);
        }
        raw.clear();
        state = 0;
        continue;
      }
      raw.push_back(b);
    }
    vTaskDelay(1);
  }
}

void onReadlData() {
  uint8_t data[9];
  Serial1.readBytes(data, 9);
  portENTER_CRITICAL(&jw01_spinlock);
  if(data[8]==(uint8_t)(data[0]+data[1]+data[2]+data[3]+data[4]+data[5]+data[6]+data[7])){
    //Serial.println("Checksum OK");
    // Serial.printf("CH2O:");
    // Serial.print((data[4]*256+data[5])*0.01);
    CH2O = (data[4]*256+data[5])*0.01;
    // Serial.printf(" TVOC:");
    // Serial.print((data[2]*256+data[3])*0.01);
    TVOC = (data[2]*256+data[3])*0.01;
    //Serial.print(" CO2:");
    //Serial.print(data[6]*256+data[7]);
    CO2 = data[6]*256+data[7];
    //Serial.println();
  }
  portEXIT_CRITICAL(&jw01_spinlock);
}

void dhtRead(float &h, float &t) {
  h = dht.readHumidity();
  t = dht.readTemperature();
  if (isnan(h) || isnan(t)) {
    //Serial.println("Failed to read from DHT sensor!");
    return;
  }
}

void send_data(void *pvParameters) {
  float h = 0, t = 0;
  char buffer[100]; // Increased buffer size to accommodate all values
  while (true) {
    dhtRead(h, t);
    portENTER_CRITICAL(&jw01_spinlock);
    snprintf(buffer, sizeof(buffer), "Humi:%.3f;Temp:%.3f;CH2O:%.3f;TVOC:%.3f;CO_2:%.3f;", h, t, CH2O, TVOC, CO2);
    portEXIT_CRITICAL(&jw01_spinlock);
    // 传感器数据属于“周期性上报”，不需要 ACK；丢一次下次会再发
    SendFrameUnreliable(PROTO_SENSOR, (const uint8_t*)buffer, (uint16_t)strlen(buffer));
    vTaskDelay(1000 / portTICK_PERIOD_MS);
  }
}

void setup() {
  serialMutex = xSemaphoreCreateMutex();
  reliableMutex = xSemaphoreCreateMutex();
  ackSem = xSemaphoreCreateBinary();
  frameQueue = xQueueCreate(10, sizeof(FrameMsg*));

  Serial.begin(115200);
  pixels.begin();
  pixels.setBrightness(8);
  showPixelColor(0x0);
  initCamera();
  dht.begin();

  Serial1.begin(9600, SERIAL_8N1, RXD_PIN, UART_PIN_NO_CHANGE);
  Serial1.onReceive(onReadlData, true);

  xTaskCreate(serialRxTask, "serialRxTask", 4096, NULL, 2, NULL);
  xTaskCreate(frameHandlerTask, "frameHandlerTask", 4096, NULL, 1, NULL);
  xTaskCreate(send_data, "send_data", 4096, NULL, 1, NULL);
}

void loop() {
}