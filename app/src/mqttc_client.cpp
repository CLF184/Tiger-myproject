#include "mqttc_client.h"

#include <cerrno>
#include <cstring>
#include <cstdlib>

#include <fcntl.h>

#include <netdb.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

namespace mqttc {

namespace {
constexpr int kDefaultPort = 1883;
constexpr int kKeepAliveSeconds = 600;
constexpr int kSyncStepMs = 50;
constexpr int kConnectWaitMs = 5000;

// 发送缓冲区需要能够容纳最大一条 MQTT 报文（含主题、头部和 payload）。
// 传感器 JSON + Base64 图片的 payload 会明显大于 8KB，这里提升到 64KB。
constexpr size_t kSendBufSize = 64 * 1024;
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

static std::string MqttErrToString(enum MQTTErrors e)
{
    const char *s = mqtt_error_str(e);
    std::string out = s ? std::string(s) : std::string("(unknown)");
    out += " (" + std::to_string(static_cast<int>(e)) + ")";
    return out;
}

static std::string SocketErrnoSuffix()
{
    const int err = errno;
    if (err == 0) {
        return "";
    }
    return ", errno=" + std::to_string(err) + ": " + ErrnoToString(err);
}

static bool SetNonBlocking(int fd, std::string *errorMsg)
{
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags < 0) {
        if (errorMsg) {
            *errorMsg = std::string("fcntl(F_GETFL) failed") + SocketErrnoSuffix();
        }
        return false;
    }
    if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) < 0) {
        if (errorMsg) {
            *errorMsg = std::string("fcntl(F_SETFL,O_NONBLOCK) failed") + SocketErrnoSuffix();
        }
        return false;
    }
    return true;
}

} // namespace

MqttCClient::MqttCClient() : socketFd_(-1), mqttInitialized_(false)
{
    std::memset(&client_, 0, sizeof(client_));
    sendBuf_.resize(kSendBufSize);
    recvBuf_.resize(kRecvBufSize);
}

void MqttCClient::publish_callback_thunk(void **state, struct mqtt_response_publish *publish)
{
    if (!state || !*state || !publish) {
        return;
    }
    auto *self = static_cast<MqttCClient *>(*state);
    auto cb = self->msgCb_.load();
    if (!cb) {
        return;
    }

    // MQTT-C: topic_name is a non-null-terminated buffer (const void* + size).
    std::string topicStr;
    if (publish->topic_name && publish->topic_name_size > 0) {
        topicStr.assign(static_cast<const char *>(publish->topic_name),
                        static_cast<size_t>(publish->topic_name_size));
    }
    const char *topic = topicStr.empty() ? "" : topicStr.c_str();
    const void *data = publish->application_message;
    const size_t size = publish->application_message_size;
    cb(self->msgCbCtx_.load(), topic, data, size);
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
                *errorMsg = "mqtt_sync error: " + MqttErrToString(e);
                if (e == MQTT_ERROR_SOCKET_ERROR) {
                    *errorMsg += SocketErrnoSuffix();
                }
            }
            setLastErrorLocked(errorMsg ? *errorMsg : "mqtt_sync error");
            return false;
        }
        if (client_.error != MQTT_OK) {
            if (errorMsg) {
                *errorMsg = "mqtt client error: " + MqttErrToString(client_.error);
                if (client_.error == MQTT_ERROR_SOCKET_ERROR) {
                    *errorMsg += SocketErrnoSuffix();
                }
            }
            setLastErrorLocked(errorMsg ? *errorMsg : "mqtt client error");
            return false;
        }
        usleep(stepMs * 1000);
    }
    return true;
}

static bool IsConnectAcked(struct mqtt_client &client)
{
    struct mqtt_queued_message *msg = mqtt_mq_find(&client.mq, MQTT_CONTROL_CONNECT, NULL);
    if (msg == NULL) {
        // CONNECT 已被清理（通常意味着已完成）。
        return true;
    }
    return msg->state == MQTT_QUEUED_COMPLETE;
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

    // MQTT-C 的默认 PAL 实现按“非阻塞 socket”设计；若保持阻塞，mqtt_sync() 可能会在 recv() 上卡住很久。
    std::string nbErr;
    if (!SetNonBlocking(socketFd_, &nbErr)) {
        const std::string msg = "set non-blocking failed: " + nbErr;
        setLastErrorLocked(msg);
        if (errorMsg) {
            *errorMsg = msg;
        }
        ::close(socketFd_);
        socketFd_ = -1;
        return false;
    }

    if (!mqttInitialized_) {
        enum MQTTErrors initErr = mqtt_init(&client_, socketFd_,
            sendBuf_.data(), sendBuf_.size(),
            recvBuf_.data(), recvBuf_.size(),
            publish_callback_thunk);
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

    // mqtt-c: state 指针会以 void** 形式传给 publish callback
    client_.publish_response_callback_state = this;

    const char *user = username_.empty() ? nullptr : username_.c_str();
    const char *pass = password_.empty() ? nullptr : password_.c_str();

    enum MQTTErrors connErr = mqtt_connect(&client_,
        clientId_.empty() ? nullptr : clientId_.c_str(),
        nullptr, nullptr, 0,
        user, pass,
        MQTT_CONNECT_CLEAN_SESSION,
        kKeepAliveSeconds);

    if (connErr != MQTT_OK) {
        const std::string msg = "mqtt_connect failed: " + MqttErrToString(connErr);
        setLastErrorLocked(msg);
        if (errorMsg) {
            *errorMsg = msg;
        }
        ::close(socketFd_);
        socketFd_ = -1;
        return false;
    }

    // 等待 broker 的 CONNACK（带超时）。
    const int loops = kConnectWaitMs / kSyncStepMs;
    for (int i = 0; i < loops; i++) {
        std::string syncErr;
        if (!syncForMs(kSyncStepMs, kSyncStepMs, &syncErr)) {
            if (errorMsg) {
                *errorMsg = syncErr;
            }
            ::close(socketFd_);
            socketFd_ = -1;
            return false;
        }
        if (IsConnectAcked(client_)) {
            break;
        }
    }
    if (!IsConnectAcked(client_)) {
        const std::string msg = "CONNACK timeout (" + std::to_string(kConnectWaitMs) + "ms)";
        setLastErrorLocked(msg);
        if (errorMsg) {
            *errorMsg = msg;
        }
        ::close(socketFd_);
        socketFd_ = -1;
        return false;
    }

    setLastErrorLocked("");
    return socketFd_ >= 0 && client_.error == MQTT_OK;
}

void MqttCClient::setMessageCallback(MessageCallback cb, void *ctx)
{
    msgCb_.store(cb);
    msgCbCtx_.store(ctx);
}

bool MqttCClient::subscribe(const std::string &topic, int qos, std::string *errorMsg)
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

    if (qos < 0) qos = 0;
    if (qos > 2) qos = 2;

    enum MQTTErrors subErr = mqtt_subscribe(&client_, topic.c_str(), static_cast<uint8_t>(qos));
    if (subErr != MQTT_OK) {
        const std::string msg = "mqtt_subscribe failed: " + std::to_string(static_cast<int>(subErr));
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

bool MqttCClient::syncOnce(std::string *errorMsg)
{
    std::lock_guard<std::mutex> lock(mutex_);

    if (socketFd_ < 0) {
        const std::string msg = "not connected";
        setLastErrorLocked(msg);
        if (errorMsg) {
            *errorMsg = msg;
        }
        return false;
    }

    enum MQTTErrors e = mqtt_sync(&client_);
    if (e != MQTT_OK || client_.error != MQTT_OK) {
        std::string msg = "mqtt_sync error: " + MqttErrToString(e) +
            ", client=" + MqttErrToString(client_.error);
        if (e == MQTT_ERROR_SOCKET_ERROR || client_.error == MQTT_ERROR_SOCKET_ERROR) {
            msg += SocketErrnoSuffix();
        }
        setLastErrorLocked(msg);
        if (errorMsg) {
            *errorMsg = msg;
        }
        return false;
    }

    return true;
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
