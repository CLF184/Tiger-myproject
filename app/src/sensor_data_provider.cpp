#include "sensor_data_provider.h"

#include <cstdio>
#include <cstring>

#include "myserial.h"
#include "wifi_udp_receiver.h"

namespace {

sensor::DataChannel g_dataChannel = sensor::DataChannel::UDP;

float ParseValueFromText(const char *text, const char *key)
{
    if (text == nullptr || key == nullptr) {
        return 0.0f;
    }

    char searchKey[32] = {0};
    std::snprintf(searchKey, sizeof(searchKey), "%s:", key);

    const char *position = std::strstr(text, searchKey);
    if (position == nullptr) {
        return 0.0f;
    }

    float value = 0.0f;
    std::sscanf(position, "%*[^:]:%f", &value);
    return value;
}

float GetDataByKeyFromUdp(const char *key)
{
    constexpr size_t kUdpDataBufSize = 1024;
    char dataStr[kUdpDataBufSize] = {0};
    if (wifi_get_latest_data(dataStr, sizeof(dataStr)) != 0) {
        return 0.0f;
    }
    return ParseValueFromText(dataStr, key);
}

float GetDataByKeyFromSerial(const char *key)
{
    if (key == nullptr || key[0] == '\0') {
        return 0.0f;
    }

    constexpr size_t kSerialDataBufSize = 1024;
    char dataStr[kSerialDataBufSize] = {0};

    int dataLen = 0;
    unsigned char *data = return_recv(&dataLen);
    if (data == nullptr || dataLen <= 0) {
        delete[] data;
        return 0.0f;
    }

    size_t copyLen = static_cast<size_t>(dataLen);
    if (copyLen >= kSerialDataBufSize) {
        copyLen = kSerialDataBufSize - 1;
    }
    std::memcpy(dataStr, data, copyLen);
    dataStr[copyLen] = '\0';
    delete[] data;

    return ParseValueFromText(dataStr, key);
}

int SendCaptureFromSerial(const char *command)
{
    if (command == nullptr || command[0] == '\0') {
        return -1;
    }
    write_uart(command, static_cast<int>(std::strlen(command)));
    return 0;
}

int SendCaptureFromUdp(const char *command)
{
    if (command == nullptr || command[0] == '\0') {
        return -1;
    }
    return wifi_send_broadcast(command, static_cast<int>(std::strlen(command)));
}

} // namespace

namespace sensor {

void SetDataChannel(DataChannel channel)
{
    g_dataChannel = channel;
}

DataChannel GetDataChannel()
{
    return g_dataChannel;
}

float GetDataByKey(const char *key)
{
    if (g_dataChannel == DataChannel::SERIAL) {
        return GetDataByKeyFromSerial(key);
    }
    return GetDataByKeyFromUdp(key);
}

int SendCaptureCommand(const char *command)
{
    if (g_dataChannel == DataChannel::SERIAL) {
        return SendCaptureFromSerial(command);
    }
    return SendCaptureFromUdp(command);
}

} // namespace sensor
