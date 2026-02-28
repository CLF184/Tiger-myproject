#ifndef MQTT_GLOBAL_H
#define MQTT_GLOBAL_H

#include "mqttc_client.h"

namespace mqttc {

// 全局 MQTT-C 客户端实例：由 NAPI 层配置/连接，供设备侧其它模块复用。
MqttCClient &GetMqttClient();

} // namespace mqttc

#endif
