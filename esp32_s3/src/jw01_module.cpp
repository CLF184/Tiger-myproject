#include "jw01_module.h"

#include <driver/uart.h>
#include <Arduino.h>

namespace jw01_module {

static portMUX_TYPE s_spinlock = portMUX_INITIALIZER_UNLOCKED;
static float s_ch2o = 0.0f;
static float s_tvoc = 0.0f;
static float s_co2 = 0.0f;
static bool s_valid = false;
static uint32_t s_lastUpdateMs = 0;

static HardwareSerial* s_serial = nullptr;

static void IRAM_ATTR on_receive_isr() {
  if (!s_serial) {
    return;
  }

  // JW01 一帧 9 字节
  if (s_serial->available() < 9) {
    return;
  }

  uint8_t data[9];
  s_serial->readBytes(data, 9);

  const uint8_t checksum = static_cast<uint8_t>(data[0] + data[1] + data[2] + data[3] + data[4] + data[5] + data[6] + data[7]);
  if (data[8] != checksum) {
    return;
  }

  const float tvoc = (data[2] * 256 + data[3]) * 0.01f;
  const float ch2o = (data[4] * 256 + data[5]) * 0.01f;
  const float co2 = static_cast<float>(data[6] * 256 + data[7]);

  portENTER_CRITICAL_ISR(&s_spinlock);
  s_ch2o = ch2o;
  s_tvoc = tvoc;
  s_co2 = co2;
  s_valid = true;
  s_lastUpdateMs = static_cast<uint32_t>(xTaskGetTickCountFromISR()) * portTICK_PERIOD_MS;
  portEXIT_CRITICAL_ISR(&s_spinlock);
}

void init(HardwareSerial& serial, int rxPin) {
  s_serial = &serial;
  serial.begin(9600, SERIAL_8N1, rxPin, UART_PIN_NO_CHANGE);
  serial.onReceive(on_receive_isr, true);
}

void get(Jw01Data& out) {
  portENTER_CRITICAL(&s_spinlock);
  out.valid = s_valid;
  out.lastUpdateMs = s_lastUpdateMs;
  out.ch2o = s_ch2o;
  out.tvoc = s_tvoc;
  out.co2 = s_co2;
  portEXIT_CRITICAL(&s_spinlock);
}

} // namespace jw01_module
