#include "soil_module.h"

#include <Arduino.h>
#include <HardwareSerial.h>

#include <DFRobot_RTU.h>

namespace soil_module {

static DFRobot_RTU* s_rtu = nullptr;
static uint8_t s_addr = 0x01;
static uint32_t s_intervalMs = 1000;

static portMUX_TYPE s_spinlock = portMUX_INITIALIZER_UNLOCKED;
static SoilData s_data = {
  false,
  0,
  0,
  0,
  0,
  0,
  0,
  0,
  0,
  0,
  0,
  0,
};

static bool read_once(SoilData& out) {
  if (!s_rtu) {
    out.valid = false;
    out.lastError = 0xFF;
    return false;
  }

  uint16_t regs[9] = {0};
  const uint8_t err = s_rtu->readHoldingRegister(s_addr, 0x0000, regs, 9);
  out.lastError = err;
  out.lastReadMs = millis();

  if (err != 0) {
    out.valid = false;
    return false;
  }

  out.valid = true;
  out.moisture = regs[0] / 10.0f;
  out.temperature = static_cast<int16_t>(regs[1]) / 10.0f;
  out.ec = static_cast<float>(regs[2]);
  out.ph = regs[3] / 10.0f;
  out.n = static_cast<float>(regs[4]);
  out.p = static_cast<float>(regs[5]);
  out.k = static_cast<float>(regs[6]);
  out.salt = static_cast<float>(regs[7]);
  out.tds = static_cast<float>(regs[8]);

  return true;
}

static void poll_task(void* pvParameters) {
  (void)pvParameters;

  SoilData newData;
  while (true) {
    read_once(newData);

    portENTER_CRITICAL(&s_spinlock);
    s_data = newData;
    portEXIT_CRITICAL(&s_spinlock);

    vTaskDelay(s_intervalMs / portTICK_PERIOD_MS);
  }
}

void init(HardwareSerial& serial,
          int rxPin,
          int txPin,
          int dePin,
          uint32_t baud,
          uint8_t addr,
          uint32_t intervalMs) {
  s_addr = addr;
  s_intervalMs = intervalMs;

  serial.begin(baud, SERIAL_8N1, rxPin, txPin);

  if (s_rtu) {
    delete s_rtu;
    s_rtu = nullptr;
  }

  if (dePin >= 0) {
    s_rtu = new DFRobot_RTU(&serial, dePin);
  } else {
    s_rtu = new DFRobot_RTU(&serial);
  }
  s_rtu->setTimeoutTimeMs(200);

  xTaskCreate(poll_task, "soil_poll", 4096, nullptr, 1, nullptr);
}

bool get(SoilData& out) {
  portENTER_CRITICAL(&s_spinlock);
  out = s_data;
  portEXIT_CRITICAL(&s_spinlock);
  return out.valid;
}

} // namespace soil_module
