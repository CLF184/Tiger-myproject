 #ifndef MQTT_PAYLOAD_BUILDER_H
#define MQTT_PAYLOAD_BUILDER_H

#include <string>

namespace mqttc {

// Set deviceId used in sensor JSON payload.
void SetMqttPayloadDeviceId(const std::string &deviceId);

// Get deviceId currently used in payload builder.
std::string GetMqttPayloadDeviceId();

// Set/Get MQTT topic prefix (namespace), e.g. "ciallo_ohos".
// This prefix is used for per-device topics and discovery announce topics.
void SetMqttTopicPrefix(const std::string &prefix);
std::string GetMqttTopicPrefix();

// Build JSON payload for sensor topic.
// Format matches the Qt client parser:
// {"deviceId":"...","timestamp":"...","sensors":{...}}
bool BuildSensorPayloadJson(bool includeImage, std::string &outJson, std::string *errMsg = nullptr);

// Build Base64 payload for image topic. Reads PHOTO_PATH and Base64-encodes it.
bool BuildImagePayloadBase64(std::string &outBase64, std::string *errMsg = nullptr);

} // namespace mqttc

#endif
