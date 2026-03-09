#include <string>
#include <vector>
#include <algorithm>
#include <cctype>
#include <ctime>
#include <cstdio>
#include <cstring>
#include <mutex>

#include "napi/native_api.h"
#include "napi/native_common.h"
#include "napi/native_node_api.h"

#include "mqttc_client.h"
#include "mqtt_global.h"

#include "mqtt_payload_builder.h"


static mqttc::MqttCClient &g_mqttClient = mqttc::GetMqttClient();

namespace {

std::mutex g_discoveryMu;
std::string g_lastAnnouncedPrefix;

static std::string SanitizeTopicSegment(const std::string &in)
{
    std::string out;
    out.reserve(in.size());
    for (unsigned char c : in) {
        if (std::isalnum(c) || c == '_' || c == '-' || c == '.') {
            out.push_back(static_cast<char>(c));
        } else {
            out.push_back('_');
        }
    }

    out.erase(std::unique(out.begin(), out.end(), [](char a, char b) { return a == '_' && b == '_'; }), out.end());
    while (!out.empty() && out.front() == '_') out.erase(out.begin());
    while (!out.empty() && out.back() == '_') out.pop_back();
    if (out.empty()) out = "unknown";
    return out;
}

static std::string EnsureDeviceId()
{
    std::string id = mqttc::GetMqttPayloadDeviceId();
    id = SanitizeTopicSegment(id);
    if (id.empty()) {
        id = "unknown";
        mqttc::SetMqttPayloadDeviceId(id);
    }
    return id;
}

static std::string NormalizePrefixInput(const std::string &input)
{
    // Accept either "<prefix>" or "<prefix>/..."; return sanitized prefix.
    if (input.empty()) return std::string();
    const size_t slash = input.find('/');
    std::string prefix = (slash == std::string::npos) ? input : input.substr(0, slash);
    prefix = SanitizeTopicSegment(prefix);
    if (prefix.empty() || prefix == "unknown") return std::string();
    return prefix;
}

static void PublishDiscoveryRetainedBestEffort()
{
    const std::string deviceId = EnsureDeviceId();

    const std::string prefix = mqttc::GetMqttTopicPrefix();
    std::string err;
    (void)g_mqttClient.publishDiscoveryAnnounceRetained(prefix, deviceId, "unionpi", &err);
}

} // namespace

static napi_value configMqtt(napi_env env, napi_callback_info info)
{
    napi_value result;
    size_t argc = 4;
    napi_value args[4];

    char brokerUrl[256] = {0};
    size_t brokerUrlLen = 0;
    char clientId[256] = {0};
    size_t clientIdLen = 0;
    char username[256] = {0};
    size_t usernameLen = 0;
    char password[256] = {0};
    size_t passwordLen = 0;

    int status = 0;

    NAPI_CALL(env, napi_get_cb_info(env, info, &argc, args, nullptr, nullptr));

    if (argc < 1) {
        status = -1;
        NAPI_CALL(env, napi_create_int32(env, status, &result));
        return result;
    }

    if (argc < 2) {
        status = -2;
        NAPI_CALL(env, napi_create_int32(env, status, &result));
        return result;
    }

    if (argc >= 1) {
        napi_valuetype t;
        NAPI_CALL(env, napi_typeof(env, args[0], &t));
        if (t == napi_string) {
            NAPI_CALL(env, napi_get_value_string_utf8(env, args[0], brokerUrl, sizeof(brokerUrl) - 1, &brokerUrlLen));
        }
    }

    if (argc >= 2) {
        napi_valuetype t;
        NAPI_CALL(env, napi_typeof(env, args[1], &t));
        if (t == napi_string) {
            NAPI_CALL(env, napi_get_value_string_utf8(env, args[1], clientId, sizeof(clientId) - 1, &clientIdLen));
        }
    }

    if (argc >= 3) {
        napi_valuetype t;
        NAPI_CALL(env, napi_typeof(env, args[2], &t));
        if (t == napi_string) {
            NAPI_CALL(env, napi_get_value_string_utf8(env, args[2], username, sizeof(username) - 1, &usernameLen));
        }
    }

    if (argc >= 4) {
        napi_valuetype t;
        NAPI_CALL(env, napi_typeof(env, args[3], &t));
        if (t == napi_string) {
            NAPI_CALL(env, napi_get_value_string_utf8(env, args[3], password, sizeof(password) - 1, &passwordLen));
        }
    }

    // default topic prefix; may be overridden later based on publish topic input.
    mqttc::SetMqttTopicPrefix("ciallo_ohos");

    std::string clientIdStr(clientId, clientIdLen);
    clientIdStr = SanitizeTopicSegment(clientIdStr);
    if (clientIdStr.empty()) {
        status = -3;
        NAPI_CALL(env, napi_create_int32(env, status, &result));
        return result;
    }

    g_mqttClient.configure(brokerUrl, clientIdStr, username, password);
    // 设备侧 deviceId 直接使用 ETS 传入的 deviceId（即第二个参数）。
    mqttc::SetMqttPayloadDeviceId(clientIdStr);
    NAPI_CALL(env, napi_create_int32(env, status, &result));
    return result;
}

struct MqttAsyncContext {
    napi_async_work work;
    napi_deferred deferred;
    bool success;
    std::string error;

    // publish args
    std::string topic;
    std::string payload;
    int qos = 0;
    bool isImage = false;
};

static void ConnectExecute(napi_env env, void *data)
{
    (void)env;
    auto *ctx = static_cast<MqttAsyncContext *>(data);
    std::string err;
    ctx->success = g_mqttClient.connect(&err);
    if (!ctx->success) {
        ctx->error = err.empty() ? g_mqttClient.getLastError() : err;
        if (ctx->error.empty()) {
            ctx->error = "connect failed";
        }
        return;
    }

    // Best-effort: publish retained discovery message once after successful connect.
    PublishDiscoveryRetainedBestEffort();
    {
        std::lock_guard<std::mutex> lock(g_discoveryMu);
        g_lastAnnouncedPrefix = mqttc::GetMqttTopicPrefix();
    }
}

static void PublishExecute(napi_env env, void *data)
{
    (void)env;
    auto *ctx = static_cast<MqttAsyncContext *>(data);

    std::string buildErr;
    // haveImage=true: 将图片(Base64)作为 JSON 的可选字段一并发送
    if (!mqttc::BuildSensorPayloadJson(ctx->isImage, ctx->payload, &buildErr)) {
        ctx->success = false;
        ctx->error = buildErr.empty() ? "build sensor payload failed" : buildErr;
        return;
    }

    std::string err;
    ctx->success = g_mqttClient.publish(ctx->topic, ctx->payload.data(), ctx->payload.size(), ctx->qos, false, &err);
    if (!ctx->success) {
        ctx->error = err.empty() ? g_mqttClient.getLastError() : err;
        if (ctx->error.empty()) {
            ctx->error = "publish failed";
        }
    }
}

static void BoolPromiseComplete(napi_env env, napi_status status, void *data)
{
    (void)status;
    auto *ctx = static_cast<MqttAsyncContext *>(data);

    if (ctx->success) {
        napi_value result;
        NAPI_CALL_RETURN_VOID(env, napi_get_boolean(env, true, &result));
        NAPI_CALL_RETURN_VOID(env, napi_resolve_deferred(env, ctx->deferred, result));
    } else {
        napi_value error;
        napi_value msg;
        NAPI_CALL_RETURN_VOID(env, napi_create_string_utf8(env, ctx->error.c_str(), ctx->error.size(), &msg));
        NAPI_CALL_RETURN_VOID(env, napi_create_error(env, nullptr, msg, &error));
        NAPI_CALL_RETURN_VOID(env, napi_reject_deferred(env, ctx->deferred, error));
    }

    NAPI_CALL_RETURN_VOID(env, napi_delete_async_work(env, ctx->work));
    delete ctx;
}

static napi_value connectMqtt(napi_env env, napi_callback_info info)
{
    (void)info;
    napi_value promise;

    auto *ctx = new MqttAsyncContext();
    ctx->success = false;

    NAPI_CALL(env, napi_create_promise(env, &ctx->deferred, &promise));

    napi_value resource_name;
    NAPI_CALL(env, napi_create_string_utf8(env, "ConnectMqtt", NAPI_AUTO_LENGTH, &resource_name));

    NAPI_CALL(env, napi_create_async_work(env, nullptr, resource_name,
        ConnectExecute, BoolPromiseComplete,
        ctx, &ctx->work));

    NAPI_CALL(env, napi_queue_async_work(env, ctx->work));
    return promise;
}

static napi_value disconnectMqtt(napi_env env, napi_callback_info info)
{
    (void)info;
    napi_value result;
    g_mqttClient.disconnect();
    NAPI_CALL(env, napi_create_int32(env, 0, &result));
    return result;
}

static napi_value isMqttConnected(napi_env env, napi_callback_info info)
{
    (void)info;
    napi_value result;
    bool connected = g_mqttClient.isConnected();
    NAPI_CALL(env, napi_get_boolean(env, connected, &result));
    return result;
}

static napi_value publishMqtt(napi_env env, napi_callback_info info)
{
    napi_value promise;
    // publishMqtt(topic, haveImage, qos?) -> Promise<boolean>
    size_t argc = 3;
    napi_value args[3];

    char prefixInput[256] = {0};
    size_t prefixInputLen = 0;
    int qos = 0;
    bool haveImage = false;

    NAPI_CALL(env, napi_get_cb_info(env, info, &argc, args, nullptr, nullptr));

    if (argc < 2) {
        napi_value error;
        napi_value msg;
        NAPI_CALL(env, napi_create_string_utf8(env, "publishMqtt(topic, haveImage, qos?) requires at least 2 args", NAPI_AUTO_LENGTH, &msg));
        NAPI_CALL(env, napi_create_error(env, nullptr, msg, &error));
        NAPI_CALL(env, napi_throw(env, error));
        return nullptr;
    }

    NAPI_CALL(env, napi_get_value_string_utf8(env, args[0], prefixInput, sizeof(prefixInput) - 1, &prefixInputLen));

    napi_valuetype haveImageType;
    NAPI_CALL(env, napi_typeof(env, args[1], &haveImageType));
    if (haveImageType != napi_boolean) {
        napi_value error;
        napi_value msg;
        NAPI_CALL(env, napi_create_string_utf8(env, "haveImage must be boolean", NAPI_AUTO_LENGTH, &msg));
        NAPI_CALL(env, napi_create_error(env, nullptr, msg, &error));
        NAPI_CALL(env, napi_throw(env, error));
        return nullptr;
    }
    NAPI_CALL(env, napi_get_value_bool(env, args[1], &haveImage));

    if (argc >= 3) {
        napi_valuetype t;
        NAPI_CALL(env, napi_typeof(env, args[2], &t));
        if (t == napi_number) {
            NAPI_CALL(env, napi_get_value_int32(env, args[2], &qos));
            if (qos < 0) qos = 0;
            if (qos > 2) qos = 2;
        }
    }

    auto *ctx = new MqttAsyncContext();
    ctx->success = false;
    {
        const std::string deviceId = EnsureDeviceId();
        const std::string input(prefixInput, prefixInputLen);

        // publishMqtt(arg0) 仅作为 topic 前缀（命名空间）使用。
        // 原生侧统一发布到 <prefix>/<deviceId>/sensors。
        std::string prefix = NormalizePrefixInput(input);
        if (!prefix.empty()) {
            mqttc::SetMqttTopicPrefix(prefix);
        } else {
            prefix = mqttc::GetMqttTopicPrefix();
            if (prefix.empty()) prefix = "ciallo_ohos";
        }

        // 若 prefix 发生变化，仅补发一次 retained announce（而不是每次上传都发）。
        if (g_mqttClient.isConnected()) {
            bool needAnnounce = false;
            {
                std::lock_guard<std::mutex> lock(g_discoveryMu);
                if (g_lastAnnouncedPrefix != prefix) {
                    needAnnounce = true;
                    g_lastAnnouncedPrefix = prefix;
                }
            }
            if (needAnnounce) {
                PublishDiscoveryRetainedBestEffort();
            }
        }

        ctx->topic = prefix + "/" + deviceId + "/sensors";
    }
    ctx->qos = qos;
    ctx->isImage = haveImage;

    NAPI_CALL(env, napi_create_promise(env, &ctx->deferred, &promise));

    napi_value resource_name;
    NAPI_CALL(env, napi_create_string_utf8(env, "PublishMqtt", NAPI_AUTO_LENGTH, &resource_name));

    NAPI_CALL(env, napi_create_async_work(env, nullptr, resource_name,
        PublishExecute, BoolPromiseComplete,
        ctx, &ctx->work));

    NAPI_CALL(env, napi_queue_async_work(env, ctx->work));
    return promise;
}

napi_value RegisterMqttApis(napi_env env, napi_value exports)
{
    napi_property_descriptor desc[] = {
        DECLARE_NAPI_FUNCTION("configMqtt", configMqtt),
        DECLARE_NAPI_FUNCTION("connectMqtt", connectMqtt),
        DECLARE_NAPI_FUNCTION("disconnectMqtt", disconnectMqtt),
        DECLARE_NAPI_FUNCTION("isMqttConnected", isMqttConnected),
        DECLARE_NAPI_FUNCTION("publishMqtt", publishMqtt),
    };

    NAPI_CALL(env, napi_define_properties(env, exports, sizeof(desc) / sizeof(desc[0]), desc));
    return exports;
}
