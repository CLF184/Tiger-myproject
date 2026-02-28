#include "mqtt_global.h"

namespace mqttc {

MqttCClient &GetMqttClient()
{
    static MqttCClient client;
    return client;
}

} // namespace mqttc
