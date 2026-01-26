#include "mqttc_client.h"

#include <cerrno>
#include <cstring>
#include <cstdlib>

#include <netdb.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

namespace mqttc {

namespace {
constexpr int kDefaultPort = 1883;
constexpr int kKeepAliveSeconds = 600;
constexpr int kSyncStepMs = 50;

constexpr size_t kSendBufSize = 8 * 1024;
constexpr size_t kRecvBufSize = 8 * 1024;

static uint8_t PublishFlagsForQos(int qos, bool retain)
{
    uint8_t flags = 0;
    switch (qos) {
        case 0:
            flags |= MQTT_PUBLISH_QOS_0;
            break;
        case 1:
            flags |= MQTT_PUBLISH_QOS_1;
            break;
        case 2:
            flags |= MQTT_PUBLISH_QOS_2;
            break;
        default:
            flags |= MQTT_PUBLISH_QOS_0;
            break;
    }
    if (retain) {
        flags |= MQTT_PUBLISH_RETAIN;
    }
    return flags;
}

static std::string ErrnoToString(int err)
{
    char buf[256];
    buf[0] = '\0';
    strerror_r(err, buf, sizeof(buf));
    return std::string(buf);
}

static void noop_publish_callback(void ** /*state*/, struct mqtt_response_publish * /*publish*/)
{
}

} // namespace

MqttCClient::MqttCClient() : socketFd_(-1), mqttInitialized_(false)
{
    std::memset(&client_, 0, sizeof(client_));
    sendBuf_.resize(kSendBufSize);
    recvBuf_.resize(kRecvBufSize);
}

MqttCClient::~MqttCClient()
{
    disconnect();
}

void MqttCClient::configure(const std::string &brokerUrl,
                            const std::string &clientId,
                            const std::string &username,
                            const std::string &password)
{
    std::lock_guard<std::mutex> lock(mutex_);
    brokerUrl_ = brokerUrl;
    clientId_ = clientId;
    username_ = username;
    password_ = password;
}

bool MqttCClient::ParseBrokerUrl(const std::string &brokerUrl, std::string &hostOut, int &portOut)
{
    std::string url = brokerUrl;

    // Accept formats:
    //  - tcp://host:port
    //  - host:port
    //  - host
    const std::string tcpPrefix = "tcp://";
    if (url.rfind(tcpPrefix, 0) == 0) {
        url = url.substr(tcpPrefix.size());
    }

    // Strip any path part (e.g., tcp://host:port/xxx)
    auto slashPos = url.find('/');
    if (slashPos != std::string::npos) {
        url = url.substr(0, slashPos);
    }

    if (url.empty()) {
        return false;
    }

    std::string host = url;
    int port = kDefaultPort;

    auto colonPos = url.rfind(':');
    if (colonPos != std::string::npos && colonPos + 1 < url.size()) {
        host = url.substr(0, colonPos);
        std::string portStr = url.substr(colonPos + 1);
        char *endp = nullptr;
        long p = std::strtol(portStr.c_str(), &endp, 10);
        if (endp != nullptr && *endp == '\0' && p > 0 && p <= 65535) {
            port = static_cast<int>(p);
        } else {
            port = kDefaultPort;
        }
    }

    if (host.empty()) {
        return false;
    }

    hostOut = host;
    portOut = port;
    return true;
}

int MqttCClient::OpenSocket(const std::string &host, int port, std::string *errorMsg)
{
    struct addrinfo hints;
    std::memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    struct addrinfo *result = nullptr;
    const std::string portStr = std::to_string(port);
    int rc = getaddrinfo(host.c_str(), portStr.c_str(), &hints, &result);
    if (rc != 0) {
        if (errorMsg) {
            *errorMsg = std::string("getaddrinfo failed: ") + gai_strerror(rc);
        }
        return -1;
    }

    int sock = -1;
    for (struct addrinfo *rp = result; rp != nullptr; rp = rp->ai_next) {
        sock = ::socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
        if (sock == -1) {
            continue;
        }

        if (::connect(sock, rp->ai_addr, rp->ai_addrlen) == 0) {
            break;
        }

        int err = errno;
        ::close(sock);
        sock = -1;
        if (errorMsg) {
            *errorMsg = std::string("connect failed: ") + ErrnoToString(err);
        }
    }

    freeaddrinfo(result);

    if (sock == -1 && errorMsg && errorMsg->empty()) {
        *errorMsg = "failed to connect";
    }

    return sock;
}

void MqttCClient::setLastErrorLocked(const std::string &errorMsg)
{
    lastError_ = errorMsg;
}

std::string MqttCClient::getLastError() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return lastError_;
}

bool MqttCClient::isConnected() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return socketFd_ >= 0 && client_.error == MQTT_OK;
}

bool MqttCClient::syncForMs(int totalMs, int stepMs, std::string *errorMsg)
{
    const int loops = (stepMs > 0) ? (totalMs / stepMs) : 0;
    for (int i = 0; i < loops; i++) {
        enum MQTTErrors e = mqtt_sync(&client_);
        if (e != MQTT_OK) {
            if (errorMsg) {
                *errorMsg = "mqtt_sync error: " + std::to_string(static_cast<int>(e));
            }
            setLastErrorLocked(errorMsg ? *errorMsg : "mqtt_sync error");
            return false;
        }
        if (client_.error != MQTT_OK) {
            if (errorMsg) {
                *errorMsg = "mqtt client error: " + std::to_string(static_cast<int>(client_.error));
            }
            setLastErrorLocked(errorMsg ? *errorMsg : "mqtt client error");
            return false;
        }
        usleep(stepMs * 1000);
    }
    return true;
}

bool MqttCClient::connect(std::string *errorMsg)
{
    std::lock_guard<std::mutex> lock(mutex_);

    if (socketFd_ >= 0 && client_.error == MQTT_OK) {
        return true;
    }

    std::string host;
    int port = kDefaultPort;
    if (!ParseBrokerUrl(brokerUrl_, host, port)) {
        const std::string msg = "invalid brokerUrl: " + brokerUrl_;
        setLastErrorLocked(msg);
        if (errorMsg) {
            *errorMsg = msg;
        }
        return false;
    }

    std::string sockErr;
    int sock = OpenSocket(host, port, &sockErr);
    if (sock < 0) {
        const std::string msg = sockErr.empty() ? "open socket failed" : sockErr;
        setLastErrorLocked(msg);
        if (errorMsg) {
            *errorMsg = msg;
        }
        return false;
    }

    socketFd_ = sock;

    if (!mqttInitialized_) {
        enum MQTTErrors initErr = mqtt_init(&client_, socketFd_,
            sendBuf_.data(), sendBuf_.size(),
            recvBuf_.data(), recvBuf_.size(),
            noop_publish_callback);
        if (initErr != MQTT_OK) {
            const std::string msg = "mqtt_init failed: " + std::to_string(static_cast<int>(initErr));
            setLastErrorLocked(msg);
            if (errorMsg) {
                *errorMsg = msg;
            }
            ::close(socketFd_);
            socketFd_ = -1;
            return false;
        }
        mqttInitialized_ = true;
    } else {
        mqtt_reinit(&client_, socketFd_,
            sendBuf_.data(), sendBuf_.size(),
            recvBuf_.data(), recvBuf_.size());
    }

    const char *user = username_.empty() ? nullptr : username_.c_str();
    const char *pass = password_.empty() ? nullptr : password_.c_str();

    enum MQTTErrors connErr = mqtt_connect(&client_,
        clientId_.empty() ? nullptr : clientId_.c_str(),
        nullptr, nullptr, 0,
        user, pass,
        MQTT_CONNECT_CLEAN_SESSION,
        kKeepAliveSeconds);

    if (connErr != MQTT_OK) {
        const std::string msg = "mqtt_connect failed: " + std::to_string(static_cast<int>(connErr));
        setLastErrorLocked(msg);
        if (errorMsg) {
            *errorMsg = msg;
        }
        ::close(socketFd_);
        socketFd_ = -1;
        return false;
    }

    // Let mqtt_sync process CONNACK and settle state.
    std::string syncErr;
    if (!syncForMs(1000, kSyncStepMs, &syncErr)) {
        if (errorMsg) {
            *errorMsg = syncErr;
        }
        ::close(socketFd_);
        socketFd_ = -1;
        return false;
    }

    setLastErrorLocked("");
    return socketFd_ >= 0 && client_.error == MQTT_OK;
}

void MqttCClient::disconnect()
{
    std::lock_guard<std::mutex> lock(mutex_);

    if (socketFd_ < 0) {
        return;
    }

    (void)mqtt_disconnect(&client_);
    (void)mqtt_sync(&client_);

    ::close(socketFd_);
    socketFd_ = -1;
}

bool MqttCClient::publish(const std::string &topic,
                          const void *data,
                          size_t size,
                          int qos,
                          bool retain,
                          std::string *errorMsg)
{
    std::lock_guard<std::mutex> lock(mutex_);

    if (socketFd_ < 0 || client_.error != MQTT_OK) {
        const std::string msg = "not connected";
        setLastErrorLocked(msg);
        if (errorMsg) {
            *errorMsg = msg;
        }
        return false;
    }

    uint8_t flags = PublishFlagsForQos(qos, retain);
    enum MQTTErrors pubErr = mqtt_publish(&client_, topic.c_str(), data, size, flags);

    if (pubErr != MQTT_OK) {
        const std::string msg = "mqtt_publish failed: " + std::to_string(static_cast<int>(pubErr));
        setLastErrorLocked(msg);
        if (errorMsg) {
            *errorMsg = msg;
        }
        return false;
    }

    std::string syncErr;
    if (!syncForMs(500, kSyncStepMs, &syncErr)) {
        if (errorMsg) {
            *errorMsg = syncErr;
        }
        return false;
    }

    setLastErrorLocked("");
    return true;
}

} // namespace mqttc
