#pragma once

#include <stdint.h>

namespace dht_module {

struct DhtData {
	bool valid;
	float humidity;
	float temperature;
};

void init(uint8_t pin, uint8_t type);

// 返回 true 表示读数有效（非 NaN）
bool read(DhtData& out);

} // namespace dht_module
