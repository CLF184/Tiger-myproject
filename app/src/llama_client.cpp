#include "llama_client.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <poll.h>
#include <string.h>
#include <iostream>
#include <sstream>
#include "llama_client.h"

#include "mqtt_payload_builder.h" // mqttc::BuildSensorPayloadJson

#include "cJSON.h"
// #include "hilog/log.h"

// #define LOG_TAG "LlamaClient"
// #define LOG_DOMAIN 0xD002300
// #define LOGI(...) ::OHOS::HiviewDFX::HiLog::Info(::OHOS::HiviewDFX::HiLogLabel{LOG_DOMAIN, 0, LOG_TAG}, __VA_ARGS__)
// #define LOGE(...) ::OHOS::HiviewDFX::HiLog::Error(::OHOS::HiviewDFX::HiLogLabel{LOG_DOMAIN, 0, LOG_TAG}, __VA_ARGS__)

namespace llama {

static std::string BuildEnvContextSystemMessageSnippet(bool *okOut = nullptr)
{
    if (okOut) {
        *okOut = false;
    }

    std::string json;
    std::string err;
    if (!mqttc::BuildSensorPayloadJson(false /* includeImage */, json, &err)) {
        return std::string();
    }

    if (okOut) {
        *okOut = true;
    }
    return std::string("当前设备环境信息(JSON)：\n") + json;
}

static std::string CreateJsonRequestCjson(const LlamaRequestParams& params,
                                         const std::vector<std::pair<std::string, std::string>>& messageHistory,
                                         const std::string& systemMessage,
                                         float temperature,
                                         int max_tokens)
{
    cJSON *root = cJSON_CreateObject();
    if (root == nullptr) {
        return "{}";
    }

    bool ok = true;
    cJSON *messages = cJSON_AddArrayToObject(root, "messages");
    ok = ok && (messages != nullptr);

    auto addMessage = [&ok, messages](const char *role, const std::string &content) {
        if (!ok) return;
        cJSON *msg = cJSON_CreateObject();
        if (msg == nullptr) {
            ok = false;
            return;
        }
        if (cJSON_AddStringToObject(msg, "role", role) == nullptr ||
            cJSON_AddStringToObject(msg, "content", content.c_str()) == nullptr) {
            cJSON_Delete(msg);
            ok = false;
            return;
        }
        cJSON_AddItemToArray(messages, msg);
    };

    if (!systemMessage.empty()) {
        addMessage("system", systemMessage);
    }

    for (const auto &it : messageHistory) {
        addMessage(it.first.c_str(), it.second);
    }

    if (!params.prompt.empty()) {
        addMessage("user", params.prompt);
    }

    ok = ok && (cJSON_AddNumberToObject(root, "temperature", static_cast<double>(temperature)) != nullptr);
    ok = ok && (cJSON_AddNumberToObject(root, "max_tokens", max_tokens) != nullptr);

    if (!ok) {
        cJSON_Delete(root);
        return "{}";
    }

    char *printed = cJSON_PrintUnformatted(root);
    if (printed == nullptr) {
        cJSON_Delete(root);
        return "{}";
    }
    std::string out(printed);
    cJSON_free(printed);
    cJSON_Delete(root);
    return out;
}

// 简单的JSON构建函数，使用OpenAI API格式
std::string createJsonRequest(const LlamaRequestParams& params,
                              const std::vector<std::pair<std::string, std::string>>& messageHistory,
                              const std::string& systemMessage = "",
                              float temperature = 0.8f,
                              int max_tokens = 1024) {
    return CreateJsonRequestCjson(params, messageHistory, systemMessage, temperature, max_tokens);
}

LlamaClient::LlamaClient(const std::string& host, int port, const std::string& systemMessage,
                         float temperature, int max_tokens)
    : m_host(host), m_port(port), m_socket(-1), m_connected(false),
      m_systemMessage(systemMessage), m_temperature(temperature), m_maxTokens(max_tokens) {
    // LOGI("LlamaClient initialized with host=%{public}s, port=%{public}d", host.c_str(), port);
}

LlamaClient::~LlamaClient() {
    disconnect();
    // LOGI("LlamaClient destroyed");
}

bool LlamaClient::connect() {
    // 如果已连接，先断开
    if (m_connected) {
        disconnect();
    }

    // 创建socket
    m_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (m_socket < 0) {
        setLastError("Failed to create socket");
        // LOGE("Failed to create socket: %{public}s", strerror(errno));
        return false;
    }

    // 设置连接地址
    struct sockaddr_in serverAddr;
    memset(&serverAddr, 0, sizeof(serverAddr));
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(m_port);
    
    if (inet_pton(AF_INET, m_host.c_str(), &serverAddr.sin_addr) <= 0) {
        setLastError("Invalid address");
        // LOGE("Invalid address: %{public}s", strerror(errno));
        close(m_socket);
        m_socket = -1;
        return false;
    }

    // 连接到服务器
    if (::connect(m_socket, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) < 0) {
        setLastError("Connection failed");
        // LOGE("Connection failed: %{public}s", strerror(errno));
        close(m_socket);
        m_socket = -1;
        return false;
    }

    m_connected = true;
    // LOGI("Connected to LLaMA server at %{public}s:%{public}d", m_host.c_str(), m_port);
    return true;
}

void LlamaClient::disconnect() {
    if (m_socket >= 0) {
        close(m_socket);
        m_socket = -1;
        m_connected = false;
        // LOGI("Disconnected from LLaMA server");
    }
}

bool LlamaClient::isConnected() const {
    return m_connected;
}

std::string LlamaClient::serializeRequest(const LlamaRequestParams& params) {
    std::string systemMessage = m_systemMessage;

    if (params.includeEnvContext) {
        if (!params.plantName.empty()) {
            if (!systemMessage.empty()) {
                systemMessage.append("\n\n");
            }
            systemMessage.append(std::string("当前种植植物：") + params.plantName);
        }

        bool ok = false;
        std::string envSnippet = BuildEnvContextSystemMessageSnippet(&ok);
        if (ok && !envSnippet.empty()) {
            if (!systemMessage.empty()) {
                systemMessage.append("\n\n");
            }
            systemMessage.append(envSnippet);
        }
    }

    // 使用内部存储的消息历史、温度和最大令牌数创建请求
    return createJsonRequest(params, m_messageHistory, systemMessage, m_temperature, m_maxTokens);
}

LlamaResponse LlamaClient::parseResponse(const std::string& json) {
    LlamaResponse response;

    if (json.empty()) {
        return response;
    }

    cJSON *root = cJSON_Parse(json.c_str());
    if (root == nullptr) {
        response.error = "Invalid JSON response";
        return response;
    }

    cJSON *id = cJSON_GetObjectItemCaseSensitive(root, "id");
    if (cJSON_IsString(id) && id->valuestring != nullptr) {
        response.id = id->valuestring;
    }

    cJSON *err = cJSON_GetObjectItemCaseSensitive(root, "error");
    if (cJSON_IsObject(err)) {
        cJSON *msg = cJSON_GetObjectItemCaseSensitive(err, "message");
        if (cJSON_IsString(msg) && msg->valuestring != nullptr) {
            response.error = msg->valuestring;
        } else {
            response.error = "Server returned error";
        }
        cJSON_Delete(root);
        return response;
    }

    cJSON *choices = cJSON_GetObjectItemCaseSensitive(root, "choices");
    if (!cJSON_IsArray(choices) || cJSON_GetArraySize(choices) <= 0) {
        cJSON_Delete(root);
        return response;
    }

    cJSON *choice0 = cJSON_GetArrayItem(choices, 0);
    if (!cJSON_IsObject(choice0)) {
        cJSON_Delete(root);
        return response;
    }

    cJSON *finishReason = cJSON_GetObjectItemCaseSensitive(choice0, "finish_reason");
    if (finishReason != nullptr && !cJSON_IsNull(finishReason)) {
        response.finished = true;
    }
    if (cJSON_IsString(finishReason) && finishReason->valuestring != nullptr) {
        std::string reason = finishReason->valuestring;
        if (reason == "stop") {
            response.finished = true;
        }
    }

    // 非流式：choices[0].message.content / role
    cJSON *message = cJSON_GetObjectItemCaseSensitive(choice0, "message");
    if (cJSON_IsObject(message)) {
        cJSON *content = cJSON_GetObjectItemCaseSensitive(message, "content");
        if (cJSON_IsString(content) && content->valuestring != nullptr) {
            response.text = content->valuestring;
        }
        cJSON *role = cJSON_GetObjectItemCaseSensitive(message, "role");
        if (cJSON_IsString(role) && role->valuestring != nullptr) {
            response.role = role->valuestring;
        }
    }

    cJSON_Delete(root);
    return response;
}

bool LlamaClient::sendRequest(const LlamaRequestParams& params, ResponseCallback callback) {
    if (!m_connected) {
        if (!connect()) {
            return false;
        }
    }

    // 创建封装回调的lambda，用于在处理响应的同时更新消息历史
    ResponseCallback historyTrackingCallback = [this, params, callback](const LlamaResponse& response) {
        // 先调用原始回调
        callback(response);
        
        // 如果响应包含有效文本且没有错误，更新消息历史
        if (!response.text.empty() && response.error.empty()) {
            this->updateMessageHistory(params, response);
        }
    };

    // 构建HTTP请求
    std::string jsonPayload = serializeRequest(params);
    std::stringstream requestStream;
    requestStream << "POST /v1/chat/completions HTTP/1.1\r\n"
                  << "Host: " << m_host << ":" << m_port << "\r\n"
                  << "Content-Type: application/json\r\n"
                  << "Content-Length: " << jsonPayload.length() << "\r\n"
                  << "Connection: keep-alive\r\n\r\n"
                  << jsonPayload;
    
    std::string request = requestStream.str();
    
    // 发送请求
    if (send(m_socket, request.c_str(), request.length(), 0) < 0) {
        setLastError("Failed to send request");
        // LOGE("Failed to send request: %{public}s", strerror(errno));
        return false;
    }

    // LOGI("Request sent to LLaMA server");
    
    // 设置非阻塞模式
    int flags = fcntl(m_socket, F_GETFL, 0);
    fcntl(m_socket, F_SETFL, flags | O_NONBLOCK);
    
    // 使用poll监听响应
    struct pollfd fds;
    fds.fd = m_socket;
    fds.events = POLLIN;
    
    char buffer[4096];
    std::string responseData;
    bool headerParsed = false;
    size_t contentLength = 0;
    
    // 读取响应
    while (true) {
        int ret = poll(&fds, 1, 100); // 100ms超时
        
        if (ret < 0) {
            setLastError("Poll failed");
            // LOGE("Poll failed: %{public}s", strerror(errno));
            return false;
        }
        
        if (ret == 0) {
            // 超时，继续等待
            continue;
        }
        
        if (fds.revents & POLLIN) {
            int bytesRead = recv(m_socket, buffer, sizeof(buffer) - 1, 0);
            
            if (bytesRead <= 0) {
                if (bytesRead == 0) {
                    // 连接关闭
                    m_connected = false;
                    setLastError("Connection closed by server");
                    // LOGE("Connection closed by server");
                } else if (errno == EAGAIN || errno == EWOULDBLOCK) {
                    // 非阻塞模式下没有数据可读，继续等待
                    continue;
                } else {
                    // 发生错误
                    setLastError("Failed to receive data");
                    // LOGE("Failed to receive data: %{public}s", strerror(errno));
                }
                return false;
            }
            
            buffer[bytesRead] = '\0';
            responseData.append(buffer, bytesRead);
            
            // 解析HTTP头
            if (!headerParsed) {
                size_t headerEnd = responseData.find("\r\n\r\n");
                if (headerEnd != std::string::npos) {
                    headerParsed = true;
                    std::string header = responseData.substr(0, headerEnd);
                    
                    // 查找Content-Length
                    size_t contentLengthPos = header.find("Content-Length:");
                    if (contentLengthPos != std::string::npos) {
                        contentLengthPos += 15; // "Content-Length:"的长度
                        size_t endPos = header.find("\r\n", contentLengthPos);
                        std::string lengthStr = header.substr(contentLengthPos, endPos - contentLengthPos);
                        contentLength = std::stoi(lengthStr);
                    }
                    
                    // 跳过HTTP头
                    responseData = responseData.substr(headerEnd + 4);
                }
            }
            
            // 检查是否接收完整的响应
            if (headerParsed) {
                // 等待接收完整数据
                if (contentLength > 0 && responseData.length() >= contentLength) {
                    LlamaResponse response = parseResponse(responseData);
                    historyTrackingCallback(response);
                    return true;
                }
            }
        }
    }
    
    return true;
}

LlamaResponse LlamaClient::sendRequestSync(const LlamaRequestParams& params, int timeout_ms) {
    LlamaResponse finalResponse;
    bool requestComplete = false;
    
    // 使用lambda捕获响应
    auto callback = [&finalResponse, &requestComplete](const LlamaResponse& response) {
        // 合并响应内容，而不是覆盖
        if (finalResponse.text.empty()) {
            finalResponse = response;
        } else if (!response.text.empty()) {
            finalResponse.text += response.text;
        }
        
        // 更新状态信息
        finalResponse.finished = response.finished;
        if (!response.error.empty()) {
            finalResponse.error = response.error;
        }
        
        // 如果收到完成标志或错误，标记请求完成
        if (response.finished || !response.error.empty()) {
            requestComplete = true;
        }
    };
    
    // 发送请求
    if (!connect()) {
        finalResponse.error = getLastError();
        return finalResponse;
    }
    
    if (!sendRequest(params, callback)) {
        finalResponse.error = getLastError();
        return finalResponse;
    }
    
    // 等待请求完成或超时
    int elapsed = 0;
    const int sleepMs = 1000;
    while (!requestComplete && elapsed < timeout_ms) {
        usleep(sleepMs * 1000);
        elapsed += sleepMs;
        
        // 接收并处理任何待处理响应
        char buffer[4096];
        int bytesRead = recv(m_socket, buffer, sizeof(buffer) - 1, 0);
        
        if (bytesRead > 0) {
            // 收到数据，需要处理
            buffer[bytesRead] = '\0';
            std::string responseData(buffer, bytesRead);
            
            // 尝试查找HTTP头和JSON内容
            size_t headerEnd = responseData.find("\r\n\r\n");
            if (headerEnd != std::string::npos) {
                std::string jsonContent = responseData.substr(headerEnd + 4);
                LlamaResponse response = parseResponse(jsonContent);
                callback(response);
            }
        } else if (bytesRead == 0) {
            // 服务器关闭了连接，请求应该完成了
            m_connected = false;
            if (!requestComplete && finalResponse.text.empty()) {
                finalResponse.error = "Connection closed by server without response";
            }
            requestComplete = true;
            break;
        } else if (bytesRead < 0 && errno != EAGAIN && errno != EWOULDBLOCK) {
            // 真正的错误发生
            finalResponse.error = "Error receiving data: " + std::string(strerror(errno));
            requestComplete = true;
            break;
        }
    }
    
    if (!requestComplete) {
        disconnect(); // 超时时断开连接
        finalResponse.error = "Request timed out after " + std::to_string(timeout_ms) + "ms";
    }
    
    // 更新消息历史 - 修复条件，只有当响应没有错误且有文本时才更新
    if (finalResponse.error.empty() && !finalResponse.text.empty()) {
        updateMessageHistory(params, finalResponse);
    }
    
    return finalResponse;
}

std::string LlamaClient::getLastError() const {
    return m_lastError;
}

void LlamaClient::setLastError(const std::string& error) {
    m_lastError = error;
}

void LlamaClient::clearHistory() {
    m_messageHistory.clear();
}

const std::vector<std::pair<std::string, std::string>>& LlamaClient::getHistory() const {
    return m_messageHistory;
}

void LlamaClient::updateMessageHistory(const LlamaRequestParams& params, const LlamaResponse& response) {
    // 添加用户消息到历史
    if (!params.prompt.empty()) {
        // 添加为用户消息
        m_messageHistory.push_back({"user", params.prompt});
    }
    
    // 添加助手响应到历史
    if (!response.text.empty()) {
        std::string role = response.role.empty() ? "assistant" : response.role;
        m_messageHistory.push_back({role, response.text});
    }
}

} // namespace llama
