#include "dht_module.h"

#include <DHT.h>
#include <cmath>

namespace dht_module {

static DHT* g_dht = nullptr;

void init(uint8_t pin, uint8_t type) {
  if (!g_dht) {
    g_dht = new DHT(pin, type);
  }
  g_dht->begin();
}

bool read(DhtData& out) {
  out.valid = false;
  out.humidity = 0.0f;
  out.temperature = 0.0f;

  if (!g_dht) {
    return false;
  }

  float newHumidity = g_dht->readHumidity();
  float newTemperature = g_dht->readTemperature();

  if (isnan(newHumidity) || isnan(newTemperature)) {
    return false;
  }

  out.valid = true;
  out.humidity = newHumidity;
  out.temperature = newTemperature;
  return true;
}

} // namespace dht_module
