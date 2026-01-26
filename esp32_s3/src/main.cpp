#include "esp_camera.h"
#include <Adafruit_NeoPixel.h>
#include <WiFi.h>
#include <thread>
#include <vector>
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
#define CAMERA_END 0x01 //相机

char *command;
QueueHandle_t cmdQueue = xQueueCreate(10, sizeof(command));

// Create a mutex for serial communication
SemaphoreHandle_t serialMutex = NULL;

portMUX_TYPE jw01_spinlock = portMUX_INITIALIZER_UNLOCKED;

Adafruit_NeoPixel pixels(PIX_NUM, PIN_PIXS, NEO_GRB + NEO_KHZ800);

float CH2O = 0;
float TVOC = 0;
float CO2 = 0;

void transmitData(char *buf, int len,unsigned char type){
  static char temp;
  if(xSemaphoreTake(serialMutex, portMAX_DELAY) == pdTRUE){
    Serial.write(FRAME_HEAD);
    for(int i=0; i<len; i++) {
      temp=buf[i];
      if(temp==ESC ||temp==FRAME_END||temp==FRAME_HEAD||temp==CAMERA_END){
        Serial.write(ESC);
      }
      Serial.write(temp);
    }
    Serial.write(type);
  }
  xSemaphoreGive(serialMutex);
}

void sendPhoto(void *pvParameters){
  camera_fb_t *fb = NULL;
  
  fb = esp_camera_fb_get();

  if (!fb)
  {
      //printf("Camera capture failed\n");
      return;
  }

  transmitData((char*)fb->buf,fb->len,CAMERA_END);

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

// 中断回调（仅超时触发）
void IRAM_ATTR onSerialData() {
  static char c;
  static vector<char> cmd;
  cmd.clear();
  int index = 0;
  while(Serial.available()) {
    c = Serial.read();
    cmd.push_back(c);
  }
  cmd.push_back('\0'); // 添加字符串结束符
  char *temp;
  temp=new char[cmd.size()];
  memcpy(temp, cmd.data(), cmd.size());
  //Serial.printf("Received command: %s\n", temp);
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
    transmitData(buffer, strlen(buffer), FRAME_END);
    vTaskDelay(1000 / portTICK_PERIOD_MS);
  }
}

void setup() {
  serialMutex = xSemaphoreCreateMutex();
  Serial.begin(115200);
  pixels.begin();
  pixels.setBrightness(8);
  showPixelColor(0x0);
  initCamera();
  dht.begin();
  Serial.onReceive(onSerialData, true);
  Serial1.begin(9600, SERIAL_8N1, RXD_PIN, UART_PIN_NO_CHANGE);
  Serial1.onReceive(onReadlData, true);
  xTaskCreate(processCommandshandler, "processCommandshandler", 4096, NULL, 1, NULL);
  xTaskCreate(send_data, "send_data", 4096, NULL, 1, NULL);
}

void loop() {
}