#ifndef MQTT_C_CLIENT_WRAPPER_H
#define MQTT_C_CLIENT_WRAPPER_H

#include <cstddef>
#include <cstdint>
#include <mutex>
#include <string>
#include <vector>
#include <atomic>

extern "C" {
#include "mqtt.h"
}

namespace mqttc {

class MqttCClient {
public:
    using MessageCallback = void (*)(void *ctx, const char *topic, const void *data, size_t size);

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

    bool subscribe(const std::string &topic, int qos = 0, std::string *errorMsg = nullptr);

    // 发布设备发现(announce)消息：topic 为 <prefix>/announce/<deviceId>，payload 为 JSON。
    // retain=true 便于客户端(如 Qt)订阅后立即获取最近一次 announce。
    bool publishDiscoveryAnnounceRetained(const std::string &topicPrefix,
                                          const std::string &deviceId,
                                          const std::string &deviceType,
                                          std::string *errorMsg = nullptr);

    // 单次 pump：处理读写与回调分发。建议在外部循环中周期性调用。
    bool syncOnce(std::string *errorMsg = nullptr);

    void setMessageCallback(MessageCallback cb, void *ctx);

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

    std::atomic<MessageCallback> msgCb_{nullptr};
    std::atomic<void *> msgCbCtx_{nullptr};

    static void publish_callback_thunk(void **state, struct mqtt_response_publish *publish);
};

} // namespace mqttc

#endif
