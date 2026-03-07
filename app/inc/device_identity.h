#ifndef DEVICE_IDENTITY_H
#define DEVICE_IDENTITY_H

#include <string>

namespace device_identity {

// Best-effort unique id for the board/chip.
// Implementation tries several OS-provided identifiers and falls back to a non-empty string.
std::string GetBestEffortChipId();

// Return an MQTT topic-safe segment (no '/', no spaces, ASCII only). Never returns empty.
std::string SanitizeTopicSegment(const std::string &in);

// Replace placeholders in topic with deviceId:
// - "{deviceId}" or "<deviceId>" will be replaced.
// Additionally, for legacy default topics like "ciallo_ohos/sensors" or "ciallo_ohos/control",
// can expand into per-device topics when expandLegacy=true.
std::string ExpandTopicWithDeviceId(const std::string &topic,
                                    const std::string &deviceId,
                                    bool expandLegacy);

} // namespace device_identity

#endif
