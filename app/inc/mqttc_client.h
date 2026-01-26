#ifndef MQTT_C_CLIENT_WRAPPER_H
#define MQTT_C_CLIENT_WRAPPER_H

#include <cstddef>
#include <cstdint>
#include <mutex>
#include <string>
#include <vector>

extern "C" {
#include "mqtt.h"
}

namespace mqttc {

class MqttCClient {
public:
    MqttCClient();
    ~MqttCClient();

    void configure(const std::string &brokerUrl,
                   const std::string &clientId,
                   const std::string &username,
                   const std::string &password);

    bool connect(std::string *errorMsg = nullptr);
    void disconnect();

    bool publish(const std::string &topic,
                 const void *data,
                 size_t size,
                 int qos,
                 bool retain,
                 std::string *errorMsg = nullptr);

    bool isConnected() const;
    std::string getLastError() const;

private:
    static bool ParseBrokerUrl(const std::string &brokerUrl, std::string &hostOut, int &portOut);
    static int OpenSocket(const std::string &host, int port, std::string *errorMsg);

    bool syncForMs(int totalMs, int stepMs, std::string *errorMsg);
    void setLastErrorLocked(const std::string &errorMsg);

    mutable std::mutex mutex_;

    std::string brokerUrl_;
    std::string clientId_;
    std::string username_;
    std::string password_;

    int socketFd_;
    bool mqttInitialized_;
    struct mqtt_client client_;

    std::vector<uint8_t> sendBuf_;
    std::vector<uint8_t> recvBuf_;

    std::string lastError_;
};

} // namespace mqttc

#endif
