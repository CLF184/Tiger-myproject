#include "mqtt_payload_builder.h"

#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <string>
#include <vector>

#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#include "cJSON.h"

#include "myserial.h" // PHOTO_PATH
#include "sensor_data_provider.h"
#include "auto_control.h" // control::AutoControlThresholds / GetThresholds

namespace mqttc {

static std::string g_deviceId;
static std::string g_topicPrefix = "ciallo_ohos";

void SetMqttPayloadDeviceId(const std::string &deviceId)
{
    g_deviceId = deviceId;
}

std::string GetMqttPayloadDeviceId()
{
    return g_deviceId;
}

void SetMqttTopicPrefix(const std::string &prefix)
{
    if (prefix.empty()) {
        return;
    }
    g_topicPrefix = prefix;
}

std::string GetMqttTopicPrefix()
{
    return g_topicPrefix;
}

static std::string IsoTimestampUtc()
{
    // Qt::ISODate can parse "YYYY-MM-DDTHH:mm:ssZ".
    std::time_t now = std::time(nullptr);
    struct tm t;
    std::memset(&t, 0, sizeof(t));
    gmtime_r(&now, &t);

    char buf[32];
    std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", &t);
    return std::string(buf);
}

static std::string Base64Encode(const uint8_t *data, size_t len)
{
    static const char kTable[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string out;
    out.reserve(((len + 2) / 3) * 4);

    for (size_t i = 0; i < len; i += 3) {
        uint32_t v = 0;
        const int remain = static_cast<int>(len - i);

        v |= static_cast<uint32_t>(data[i]) << 16;
        if (remain > 1) v |= static_cast<uint32_t>(data[i + 1]) << 8;
        if (remain > 2) v |= static_cast<uint32_t>(data[i + 2]);

        out.push_back(kTable[(v >> 18) & 0x3F]);
        out.push_back(kTable[(v >> 12) & 0x3F]);
        out.push_back(remain > 1 ? kTable[(v >> 6) & 0x3F] : '=');
        out.push_back(remain > 2 ? kTable[v & 0x3F] : '=');
    }
    return out;
}

static bool ReadFileAll(const char *path, std::vector<uint8_t> &out, std::string &err)
{
    int fd = open(path, O_RDONLY);
    if (fd < 0) {
        err = std::string("open failed: ") + std::strerror(errno);
        return false;
    }

    struct stat st;
    if (fstat(fd, &st) != 0) {
        err = std::string("stat failed: ") + std::strerror(errno);
        close(fd);
        return false;
    }

    if (st.st_size < 0) {
        err = "invalid file size";
        close(fd);
        return false;
    }

    out.clear();
    out.resize(static_cast<size_t>(st.st_size));

    size_t off = 0;
    while (off < out.size()) {
        ssize_t n = read(fd, out.data() + off, out.size() - off);
        if (n <= 0) {
            err = std::string("read failed: ") + std::strerror(errno);
            close(fd);
            return false;
        }
        off += static_cast<size_t>(n);
    }

    close(fd);
    return true;
}

bool BuildImagePayloadBase64(std::string &outBase64, std::string *errMsg)
{
    std::vector<uint8_t> bytes;
    std::string err;
    if (!ReadFileAll(PHOTO_PATH, bytes, err)) {
        if (errMsg) *errMsg = err;
        return false;
    }

    outBase64 = Base64Encode(bytes.data(), bytes.size());
    if (outBase64.empty() && !bytes.empty()) {
        if (errMsg) *errMsg = "base64 encode failed";
        return false;
    }

    return true;
}

bool BuildSensorPayloadJson(bool includeImage, std::string &outJson, std::string *errMsg)
{
    std::string imageBase64;
    if (includeImage) {
        std::string imgErr;
        if (!BuildImagePayloadBase64(imageBase64, &imgErr)) {
            if (errMsg) *errMsg = imgErr.empty() ? "read/encode image failed" : imgErr;
            return false;
        }
    }

    // Read sensors (best-effort; keep 0 if failed)
    float soilMoistureF = sensor::GetDataByKey("SoilHumi");
    int soilMoisture = static_cast<int>(soilMoistureF + (soilMoistureF >= 0 ? 0.5f : -0.5f));

    float lightF = sensor::GetDataByKey("Light");
    int lightLevel = static_cast<int>(lightF + (lightF >= 0 ? 0.5f : -0.5f));

    float temperature = sensor::GetDataByKey("Temp");
    float humidityF = sensor::GetDataByKey("Humi");
    float formaldehyde = sensor::GetDataByKey("CH2O");
    float tvoc = sensor::GetDataByKey("TVOC");
    float co2F = sensor::GetDataByKey("CO_2");

    // 新增土壤多参数（由 ESP 返回的扩展字段）
    float soilTemp = sensor::GetDataByKey("SoilTemp");
    float ec = sensor::GetDataByKey("EC");
    float ph = sensor::GetDataByKey("pH");
    float n = sensor::GetDataByKey("N");
    float p = sensor::GetDataByKey("P");
    float k = sensor::GetDataByKey("K");
    float salt = sensor::GetDataByKey("Salt");
    float tds = sensor::GetDataByKey("TDS");

    // alarm 统一由 auto_control 提供，作为设备执行逻辑与上报显示的单一来源
    int alarm = control::GetAutoControlAlarm();

    int humidity = static_cast<int>(humidityF + (humidityF >= 0 ? 0.5f : -0.5f));
    int co2 = static_cast<int>(co2F + (co2F >= 0 ? 0.5f : -0.5f));

    const std::string deviceId = g_deviceId.empty() ? "unknown" : g_deviceId;
    const std::string ts = IsoTimestampUtc();

    cJSON *root = cJSON_CreateObject();
    if (root == nullptr) {
        if (errMsg) *errMsg = "cJSON_CreateObject failed";
        return false;
    }

    bool ok = true;
    ok = ok && (cJSON_AddStringToObject(root, "deviceId", deviceId.c_str()) != nullptr);
    ok = ok && (cJSON_AddStringToObject(root, "timestamp", ts.c_str()) != nullptr);

    cJSON *sensors = cJSON_AddObjectToObject(root, "sensors");
    ok = ok && (sensors != nullptr);
    if (ok) {
        ok = ok && (cJSON_AddNumberToObject(sensors, "soilMoisture", soilMoisture) != nullptr);
        ok = ok && (cJSON_AddNumberToObject(sensors, "lightLevel", lightLevel) != nullptr);
        ok = ok && (cJSON_AddNumberToObject(sensors, "temperature", static_cast<double>(temperature)) != nullptr);
        ok = ok && (cJSON_AddNumberToObject(sensors, "humidity", humidity) != nullptr);
        ok = ok && (cJSON_AddNumberToObject(sensors, "formaldehyde", static_cast<double>(formaldehyde)) != nullptr);
        ok = ok && (cJSON_AddNumberToObject(sensors, "tvoc", static_cast<double>(tvoc)) != nullptr);
        ok = ok && (cJSON_AddNumberToObject(sensors, "co2", co2) != nullptr);

        ok = ok && (cJSON_AddNumberToObject(sensors, "soilTemperature", static_cast<double>(soilTemp)) != nullptr);
        ok = ok && (cJSON_AddNumberToObject(sensors, "ec", static_cast<double>(ec)) != nullptr);
        ok = ok && (cJSON_AddNumberToObject(sensors, "ph", static_cast<double>(ph)) != nullptr);
        ok = ok && (cJSON_AddNumberToObject(sensors, "nitrogen", static_cast<double>(n)) != nullptr);
        ok = ok && (cJSON_AddNumberToObject(sensors, "phosphorus", static_cast<double>(p)) != nullptr);
        ok = ok && (cJSON_AddNumberToObject(sensors, "potassium", static_cast<double>(k)) != nullptr);
        ok = ok && (cJSON_AddNumberToObject(sensors, "salt", static_cast<double>(salt)) != nullptr);
        ok = ok && (cJSON_AddNumberToObject(sensors, "tds", static_cast<double>(tds)) != nullptr);
        ok = ok && (cJSON_AddNumberToObject(sensors, "alarm", alarm) != nullptr);
    }

    if (includeImage) {
        ok = ok && (cJSON_AddStringToObject(root, "image", imageBase64.c_str()) != nullptr);
    }

    if (!ok) {
        if (errMsg) *errMsg = "cJSON add field failed";
        cJSON_Delete(root);
        return false;
    }

    char *printed = cJSON_PrintUnformatted(root);
    if (printed == nullptr) {
        if (errMsg) *errMsg = "cJSON_PrintUnformatted failed";
        cJSON_Delete(root);
        return false;
    }

    outJson.assign(printed);
    cJSON_free(printed);
    cJSON_Delete(root);
    return true;
}

} // namespace mqttc
