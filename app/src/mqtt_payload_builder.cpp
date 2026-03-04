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

#include "myserial.h" // get_data_by_key + PHOTO_PATH
#include "auto_control.h" // control::AutoControlThresholds / GetThresholds

namespace mqttc {

static std::string g_deviceId;

void SetMqttPayloadDeviceId(const std::string &deviceId)
{
    g_deviceId = deviceId;
}

std::string GetMqttPayloadDeviceId()
{
    return g_deviceId;
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
    float soilMoistureF = get_data_by_key(const_cast<char *>("SoilHumi"));
    int soilMoisture = static_cast<int>(soilMoistureF + (soilMoistureF >= 0 ? 0.5f : -0.5f));

    float lightF = get_data_by_key(const_cast<char *>("Light"));
    int lightLevel = static_cast<int>(lightF + (lightF >= 0 ? 0.5f : -0.5f));

    float temperature = get_data_by_key(const_cast<char *>("Temp"));
    float humidityF = get_data_by_key(const_cast<char *>("Humi"));
    float formaldehyde = get_data_by_key(const_cast<char *>("CH2O"));
    float tvoc = get_data_by_key(const_cast<char *>("TVOC"));
    float co2F = get_data_by_key(const_cast<char *>("CO_2"));

    // 新增土壤多参数（由 ESP 返回的扩展字段）
    float soilTemp = get_data_by_key(const_cast<char *>("SoilTemp"));
    float ec = get_data_by_key(const_cast<char *>("EC"));
    float ph = get_data_by_key(const_cast<char *>("pH"));
    float n = get_data_by_key(const_cast<char *>("N"));
    float p = get_data_by_key(const_cast<char *>("P"));
    float k = get_data_by_key(const_cast<char *>("K"));
    float salt = get_data_by_key(const_cast<char *>("Salt"));
    float tds = get_data_by_key(const_cast<char *>("TDS"));

    // 简单告警判断：根据 EC、pH、N、P、K 是否超出配置的上下限给出 alarm 标志
    // alarm = 0 表示无告警；>0 表示存在至少一项超限（当前使用 1/0 即可，后续可扩展为位掩码）。
    int alarm = 0;

    auto in_range_or_unset = [](float value, double minV, double maxV) {
        // 若 min/max 均为 0，则认为未配置该项，不做告警
        if (minV == 0.0 && maxV == 0.0) {
            return true;
        }
        if (value < static_cast<float>(minV) || value > static_cast<float>(maxV)) {
            return false;
        }
        return true;
    };

    // 读取当前配置的养分/酸碱报警阈值
    control::AutoControlThresholds thresholds = control::GetThresholds();

    if (!in_range_or_unset(ph, thresholds.ph_min, thresholds.ph_max)) alarm = 1;
    if (!in_range_or_unset(ec, thresholds.ec_min, thresholds.ec_max)) alarm = 1;
    if (!in_range_or_unset(n, thresholds.n_min, thresholds.n_max)) alarm = 1;
    if (!in_range_or_unset(p, thresholds.p_min, thresholds.p_max)) alarm = 1;
    if (!in_range_or_unset(k, thresholds.k_min, thresholds.k_max)) alarm = 1;

    int humidity = static_cast<int>(humidityF + (humidityF >= 0 ? 0.5f : -0.5f));
    int co2 = static_cast<int>(co2F + (co2F >= 0 ? 0.5f : -0.5f));

    const std::string deviceId = g_deviceId.empty() ? "unknown" : g_deviceId;
    const std::string ts = IsoTimestampUtc();

    // Build JSON matching Qt/ETS client expectations，包含全部传感器数值。
    // Note: keep numeric fields as JSON numbers.
    char sensorsBuf[512];
    std::snprintf(
        sensorsBuf, sizeof(sensorsBuf),
        "\"sensors\":{"
        "\"soilMoisture\":%d,\"lightLevel\":%d,\"temperature\":%.2f,\"humidity\":%d,"
        "\"formaldehyde\":%.4f,\"tvoc\":%.4f,\"co2\":%d,"
        "\"soilTemperature\":%.2f,\"ec\":%.0f,\"ph\":%.1f,"
        "\"nitrogen\":%.0f,\"phosphorus\":%.0f,\"potassium\":%.0f,"
        "\"salt\":%.0f,\"tds\":%.0f,\"alarm\":%d}",
        soilMoisture, lightLevel, static_cast<double>(temperature), humidity,
        static_cast<double>(formaldehyde), static_cast<double>(tvoc), co2,
        static_cast<double>(soilTemp), static_cast<double>(ec), static_cast<double>(ph),
        static_cast<double>(n), static_cast<double>(p), static_cast<double>(k),
        static_cast<double>(salt), static_cast<double>(tds), alarm);

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
