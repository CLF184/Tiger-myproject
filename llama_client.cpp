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
// #include "hilog/log.h"

// #define LOG_TAG "LlamaClient"
// #define LOG_DOMAIN 0xD002300
// #define LOGI(...) ::OHOS::HiviewDFX::HiLog::Info(::OHOS::HiviewDFX::HiLogLabel{LOG_DOMAIN, 0, LOG_TAG}, __VA_ARGS__)
// #define LOGE(...) ::OHOS::HiviewDFX::HiLog::Error(::OHOS::HiviewDFX::HiLogLabel{LOG_DOMAIN, 0, LOG_TAG}, __VA_ARGS__)

namespace llama {

// 简单的JSON构建函数，使用OpenAI API格式
std::string createJsonRequest(const LlamaRequestParams& params,
                              const std::vector<std::pair<std::string, std::string>>& messageHistory,
                              const std::string& systemMessage = "",
                              float temperature = 0.8f,
                              int max_tokens = 1024,
                              const std::string& model = "gpt-3.5-turbo") {
    std::stringstream ss;
    ss << "{";
    
    // 添加model参数
    ss << "\"model\":\"" << model << "\",";
    
    ss << "\"messages\":[";
    
    // 添加system消息（如果有）
    if (!systemMessage.empty()) {
        ss << "{\"role\":\"system\",\"content\":\"" << systemMessage << "\"}";
        
        // 如果有历史或prompt，则添加逗号
        if (!messageHistory.empty() || !params.prompt.empty()) {
            ss << ",";
        }
    }
    
    // 添加消息历史
    if (!messageHistory.empty()) {
        for (size_t i = 0; i < messageHistory.size(); ++i) {
            if (i > 0) ss << ",";
            ss << "{\"role\":\"" << messageHistory[i].first << "\","
               << "\"content\":\"" << messageHistory[i].second << "\"}";
        }
        
        // 如果有新的提示，则添加逗号
        if (!params.prompt.empty()) {
            ss << ",";
        }
    }
    
    // 如果有新的提示，则添加为用户消息
    if (!params.prompt.empty()) {
        ss << "{\"role\":\"user\",\"content\":\"" << params.prompt << "\"}";
    }
    
    ss << "],";
    ss << "\"temperature\":" << temperature << ","
       << "\"max_tokens\":" << max_tokens << ","
       << "\"stream\":" << (params.stream ? "true" : "false")
       << "}";
    return ss.str();
}

LlamaClient::LlamaClient(const std::string& host, int port, const std::string& systemMessage,
                         float temperature, int max_tokens, const std::string& model)
    : m_host(host), m_port(port), m_socket(-1), m_connected(false),
      m_systemMessage(systemMessage), m_temperature(temperature), m_maxTokens(max_tokens),
      m_model(model) {
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
    // 使用内部存储的消息历史、温度、最大令牌数和模型名称创建请求
    return createJsonRequest(params, m_messageHistory, m_systemMessage, m_temperature, m_maxTokens, m_model);
}

LlamaResponse LlamaClient::parseResponse(const std::string& json) {
    LlamaResponse response;
    
    // 简单的OpenAI格式JSON解析
    // 查找id
    size_t idStart = json.find("\"id\":\"");
    if (idStart != std::string::npos) {
        idStart += 6; // "id":"的长度
        size_t idEnd = json.find("\"", idStart);
        if (idEnd != std::string::npos) {
            response.id = json.substr(idStart, idEnd - idStart);
        }
    }
    
    // 查找内容
    size_t contentStart = json.find("\"content\":\"");
    if (contentStart != std::string::npos) {
        contentStart += 11; // "content":"的长度
        size_t contentEnd = json.find("\"", contentStart);
        if (contentEnd != std::string::npos) {
            response.text = json.substr(contentStart, contentEnd - contentStart);
        }
    }
    
    // 流式传输时，可能会有delta
    if (response.text.empty()) {
        size_t deltaStart = json.find("\"delta\":{\"content\":\"");
        if (deltaStart != std::string::npos) {
            deltaStart += 20; // "delta":{"content":"的长度
            size_t deltaEnd = json.find("\"", deltaStart);
            if (deltaEnd != std::string::npos) {
                response.text = json.substr(deltaStart, deltaEnd - deltaStart);
            }
        }
    }
    
    // 查找角色
    size_t roleStart = json.find("\"role\":\"");
    if (roleStart != std::string::npos) {
        roleStart += 8; // "role":"的长度
        size_t roleEnd = json.find("\"", roleStart);
        if (roleEnd != std::string::npos) {
            response.role = json.substr(roleStart, roleEnd - roleStart);
        }
    }
    
    // 查找错误信息
    size_t errorStart = json.find("\"error\":{\"message\":\"");
    if (errorStart != std::string::npos) {
        errorStart += 19; // "error":{"message":"的长度
        size_t errorEnd = json.find("\"", errorStart);
        if (errorEnd != std::string::npos) {
            response.error = json.substr(errorStart, errorEnd - errorStart);
        }
    }
    
    // 查找结束标志
    size_t finishReasonPos = json.find("\"finish_reason\":\"");
    if (finishReasonPos != std::string::npos) {
        finishReasonPos += 16; // "finish_reason":"的长度
        size_t finishReasonEnd = json.find("\"", finishReasonPos);
        if (finishReasonEnd != std::string::npos) {
            std::string reason = json.substr(finishReasonPos, finishReasonEnd - finishReasonPos);
            response.finished = (reason == "stop");
        }
    }
    
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
    std::string accumulatedResponse; // 用于流式传输的累积响应
    
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
            if (headerParsed && !params.stream) {
                // 非流式模式，等待接收完整数据
                if (contentLength > 0 && responseData.length() >= contentLength) {
                    LlamaResponse response = parseResponse(responseData);
                    historyTrackingCallback(response);
                    return true;
                }
            } else if (headerParsed && params.stream) {
                // 流式模式，解析并回调每一块数据
                size_t pos = 0;
                size_t nextPos = responseData.find("\n", pos);
                
                while (nextPos != std::string::npos) {
                    std::string line = responseData.substr(pos, nextPos - pos);
                    pos = nextPos + 1;
                    
                    // 解析SSE数据块
                    if (line.find("data:") == 0) {
                        std::string jsonData = line.substr(5); // 跳过"data:"
                        
                        // 处理流结束标记
                        if (jsonData.find("[DONE]") != std::string::npos) {
                            LlamaResponse doneResponse;
                            doneResponse.finished = true;
                            if (!accumulatedResponse.empty()) {
                                doneResponse.text = accumulatedResponse;
                                doneResponse.role = "assistant";
                            }
                            historyTrackingCallback(doneResponse);
                            return true;
                        }
                        
                        LlamaResponse response = parseResponse(jsonData);
                        
                        // 累积文本以便在最后更新消息历史
                        if (!response.text.empty()) {
                            accumulatedResponse += response.text;
                        }
                        
                        callback(response); // 使用原始回调，不更新历史
                        
                        // 如果收到完成标志，继续处理后续数据但准备结束
                        if (response.finished) {
                            continue;
                        }
                    }
                    
                    nextPos = responseData.find("\n", pos);
                }
                
                // 保留未处理的数据
                if (pos < responseData.length()) {
                    responseData = responseData.substr(pos);
                } else {
                    responseData.clear();
                }
            }
        }
    }
    
    return true;
}

LlamaResponse LlamaClient::sendRequestSync(const LlamaRequestParams& params, int timeout_ms) {
    LlamaResponse finalResponse;
    bool requestComplete = false;
    
    // 创建一个非流式请求参数的副本
    LlamaRequestParams syncParams = params;
    syncParams.stream = false; // 确保同步请求使用非流式模式
    
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
    
    if (!sendRequest(syncParams, callback)) {
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
