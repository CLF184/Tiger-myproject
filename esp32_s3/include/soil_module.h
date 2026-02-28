#pragma once

#include <stdint.h>

class HardwareSerial;

namespace soil_module {

struct SoilData {
  bool valid;
  uint8_t lastError;
  uint32_t lastReadMs;

  float moisture;     // % (寄存器值/10)
  float temperature;  // ℃ (int16/10)
  float ec;           // uS/cm (原始)
  float ph;           // (寄存器值/10)
  float n;            // 原始寄存器值（规格为暂存/测试值）
  float p;
  float k;
  float salt;         // 仅供参考
  float tds;          // 仅供参考
};

// 使用 Modbus-RTU 读取 0x0000~0x0008（共9个保持寄存器）
// dePin: RS485 流控（收低发高），若无方向控制可传 -1
void init(HardwareSerial& serial,
          int rxPin,
          int txPin,
          int dePin,
          uint32_t baud = 4800,
          uint8_t addr = 0x01,
          uint32_t intervalMs = 1000);

// 获取最新数据快照；返回 true 表示最近一次读取成功
bool get(SoilData& out);

} // namespace soil_module
