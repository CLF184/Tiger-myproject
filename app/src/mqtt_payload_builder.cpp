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

#include "light_sensor.h"
#include "myserial.h" // get_data_by_key + PHOTO_PATH
#include "soil_moisture.h"

namespace mqttc {

static std::string g_deviceId;

void SetMqttPayloadDeviceId(const std::string &deviceId)
{
    g_deviceId = deviceId;
}

static std::string JsonEscape(const std::string &in)
{
    std::string out;
    out.reserve(in.size() + 8);
    for (char c : in) {
        switch (c) {
            case '\\': out += "\\\\"; break;
            case '"': out += "\\\""; break;
            case '\b': out += "\\b"; break;
            case '\f': out += "\\f"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default:
                if (static_cast<unsigned char>(c) < 0x20) {
                    char buf[8];
                    std::snprintf(buf, sizeof(buf), "\\u%04x", static_cast<unsigned int>(static_cast<unsigned char>(c)));
                    out += buf;
                } else {
                    out += c;
                }
        }
    }
    return out;
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
    int soilMoisture = 0;
    if (soil_moisture_read_raw(&soilMoisture) != SOIL_MOISTURE_OK) {
        soilMoisture = 0;
    }

    int lightLevel = 0;
    if (light_sensor_read(&lightLevel) != 0) {
        lightLevel = 0;
    }

    float temperature = get_data_by_key(const_cast<char *>("Temp"));
    float humidityF = get_data_by_key(const_cast<char *>("Humi"));
    float formaldehyde = get_data_by_key(const_cast<char *>("CH2O"));
    float tvoc = get_data_by_key(const_cast<char *>("TVOC"));
    float co2F = get_data_by_key(const_cast<char *>("CO_2"));

    int humidity = static_cast<int>(humidityF + (humidityF >= 0 ? 0.5f : -0.5f));
    int co2 = static_cast<int>(co2F + (co2F >= 0 ? 0.5f : -0.5f));

    const std::string deviceId = g_deviceId.empty() ? "unknown" : g_deviceId;
    const std::string ts = IsoTimestampUtc();

    // Build JSON matching Qt client expectations.
    // Note: keep numeric fields as JSON numbers.
    char sensorsBuf[256];
    std::snprintf(
        sensorsBuf, sizeof(sensorsBuf),
        "\"sensors\":{\"soilMoisture\":%d,\"lightLevel\":%d,\"temperature\":%.2f,\"humidity\":%d,\"formaldehyde\":%.4f,\"tvoc\":%.4f,\"co2\":%d}",
        soilMoisture, lightLevel, static_cast<double>(temperature), humidity,
        static_cast<double>(formaldehyde), static_cast<double>(tvoc), co2);

    outJson.clear();
    outJson.reserve(includeImage ? 512 + imageBase64.size() : 256);
    outJson += '{';
    outJson += "\"deviceId\":\"";
    outJson += JsonEscape(deviceId);
    outJson += "\",";
    outJson += "\"timestamp\":\"";
    outJson += JsonEscape(ts);
    outJson += "\",";
    outJson += sensorsBuf;

    if (includeImage) {
        outJson += ",\"image\":\"";
        outJson += imageBase64;
        outJson += "\"";
    }

    outJson += '}';

    return true;
}

} // namespace mqttc
