#pragma once

#include <stdint.h>

class HardwareSerial;

namespace jw01_module {

struct Jw01Data {
	bool valid;
	uint32_t lastUpdateMs;
	float ch2o;
	float tvoc;
	float co2;
};

// 初始化：配置串口并注册接收回调
void init(HardwareSerial& serial, int rxPin);

// 获取最近一次有效数据快照（线程安全）
void get(Jw01Data& out);

} // namespace jw01_module
