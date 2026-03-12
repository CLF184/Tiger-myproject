#ifndef SENSOR_DATA_PROVIDER_H
#define SENSOR_DATA_PROVIDER_H

namespace sensor {

enum class DataChannel {
    SERIAL = 0,
    UDP = 1,
};

// Switch sensor value source between serial and UDP backends.
void SetDataChannel(DataChannel channel);

DataChannel GetDataChannel();

// Read sensor value by key from current selected backend.
float GetDataByKey(const char *key);

// Send one command using current selected backend.
int SendCommand(const char *command);

} // namespace sensor

#endif // SENSOR_DATA_PROVIDER_H
