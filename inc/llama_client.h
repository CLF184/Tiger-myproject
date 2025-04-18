#ifndef LLAMA_CLIENT_H
#define LLAMA_CLIENT_H

#include <string>
#include <vector>
#include <functional>
#include <memory>

namespace llama {

// 定义请求参数结构体
struct LlamaRequestParams {
    std::string prompt;           // 用户输入的提示
    bool stream = false;          // 是否使用流式返回
};

// 定义响应结构体
struct LlamaResponse {
    std::string text;             // 生成的文本内容
    bool finished = false;        // 是否已完成生成
    std::string error;            // 错误信息，如果有
    std::string role;             // 回复角色(assistant)
    std::string id;               // 响应ID
};

// 回调函数类型定义
using ResponseCallback = std::function<void(const LlamaResponse&)>;

/**
 * LlamaClient 类 - 用于与局域网内LLaMA服务交互
 */
class LlamaClient {
public:
    /**
     * 构造函数
     * 
     * @param host LLaMA服务器主机名或IP地址
     * @param port LLaMA服务器端口号
     * @param systemMessage 可选的system角色消息
     * @param temperature 采样温度，默认0.8f
     * @param max_tokens 最大生成令牌数，默认1024
     */
    LlamaClient(const std::string& host = "192.168.31.5", 
                int port = 8080,
                const std::string& systemMessage = "",
                float temperature = 0.8f,
                int max_tokens = 1024);
    
    /**
     * 析构函数
     */
    ~LlamaClient();

    /**
     * 连接到LLaMA服务器
     * 
     * @return 成功返回true，失败返回false
     */
    bool connect();

    /**
     * 断开与服务器的连接
     */
    void disconnect();

    /**
     * 检查是否已连接到服务器
     * 
     * @return 已连接返回true，未连接返回false
     */
    bool isConnected() const;

    /**
     * 发送请求到LLaMA服务器
     * 
     * @param params 请求参数
     * @param callback 接收响应的回调函数
     * @return 请求成功发送返回true，失败返回false
     */
    bool sendRequest(const LlamaRequestParams& params, ResponseCallback callback);

    /**
     * 同步方式发送请求，等待并返回结果
     * 
     * @param params 请求参数
     * @param timeout_ms 超时时间(毫秒)，默认10秒
     * @return 服务器响应
     */
    LlamaResponse sendRequestSync(const LlamaRequestParams& params, int timeout_ms = 10000);

    /**
     * 获取最近的错误信息
     * 
     * @return 错误信息字符串
     */
    std::string getLastError() const;

    /**
     * 清除所有历史消息
     */
    void clearHistory();

    /**
     * 获取当前消息历史
     * 
     * @return 消息历史列表
     */
    const std::vector<std::pair<std::string, std::string>>& getHistory() const;

private:
    std::string m_host;
    int m_port;
    int m_socket;
    bool m_connected;
    std::string m_lastError;
    std::string m_systemMessage;  // system角色的消息
    std::vector<std::pair<std::string, std::string>> m_messageHistory; // 内部存储的消息历史
    float m_temperature;          // 采样温度
    int m_maxTokens;              // 最大生成令牌数

    // 序列化请求参数为JSON
    std::string serializeRequest(const LlamaRequestParams& params);
    
    // 从JSON解析响应
    LlamaResponse parseResponse(const std::string& json);
    
    // 设置最近的错误信息
    void setLastError(const std::string& error);
    
    // 更新内部消息历史
    void updateMessageHistory(const LlamaRequestParams& params, const LlamaResponse& response);
};

} // namespace llama

#endif // LLAMA_CLIENT_H
