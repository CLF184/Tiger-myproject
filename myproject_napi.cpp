/*
 * Copyright (c) 2022 Unionman Technology Co., Ltd.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <cstdio>
#include <cstring>
#include <cmath>

#include "napi/native_api.h"
#include "napi/native_node_api.h"

// 包含所有模块的头文件
#include "buzzer_control.h"
#include "fan_control.h"
#include "led_control.h"
#include "light_sensor.h"
#include "myserial.h"
#include "pump_control.h"
#include "sg90.h"
#include "soil_moisture.h"
#include "llama_client.h"  // 添加llama_client头文件

// 全局LlamaClient实例
static llama::LlamaClient* g_llamaClient = nullptr;

// 初始化所有硬件模块
static napi_value initAllModules(napi_env env, napi_callback_info info)
{
    napi_value result;
    int status = 0;
    int module_status = 0;
    
    // 初始化土壤湿度传感器
    module_status = soil_moisture_init();
    if (module_status != 0) {
        status = -1; // 土壤湿度传感器初始化失败
        goto finish;
    }
    
    // 初始化SG90舵机
    module_status = SG90_Init();
    if (module_status != 0) {
        status = -2; // SG90舵机初始化失败
        goto finish;
    }
    
    // 初始化水泵
    module_status = pump_init();
    if (module_status != 0) {
        status = -3; // 水泵初始化失败
        goto finish;
    }
    
    // 初始化光敏传感器
    module_status = light_sensor_init();
    if (module_status != 0) {
        status = -4; // 光敏传感器初始化失败
        goto finish;
    }
    
    // 初始化LED
    module_status = LedInit();
    if (module_status != 0) {
        status = -5; // LED初始化失败
        goto finish;
    }
    
    // 初始化电机控制
    module_status = initMotorControl();
    if (module_status != 0) {
        status = -6; // 电机控制初始化失败
        goto finish;
    }
    
    // 初始化蜂鸣器
    module_status = BuzzerInit();
    if (module_status != 0) {
        status = -7; // 蜂鸣器初始化失败
        goto finish;
    }
    
    // 初始化串口（无返回值，假设总是成功）
    init_uart();

finish:
    // 返回结果
    NAPI_CALL(env, napi_create_int32(env, status, &result));
    return result;
}

// 读取土壤湿度传感器原始值
static napi_value ReadSoilMoistureRaw(napi_env env, napi_callback_info info)
{
    napi_value result;
    int value = 0;
    
    if (soil_moisture_read_raw(&value) == SOIL_MOISTURE_OK) {
        NAPI_CALL(env, napi_create_int32(env, value, &result));
    } else {
        NAPI_CALL(env, napi_get_null(env, &result));
    }
    
    return result;
}

// 设置SG90舵机角度
static napi_value setSG90Angle(napi_env env, napi_callback_info info)
{
    napi_value result;
    size_t argc = 1;
    napi_value args[1];
    int angle = 0;
    int status = -1;
    
    // 获取参数
    NAPI_CALL(env, napi_get_cb_info(env, info, &argc, args, nullptr, nullptr));
    
    // 检查参数
    if (argc >= 1) {
        NAPI_CALL(env, napi_get_value_int32(env, args[0], &angle));
        // 确保角度在有效范围内
        if (angle < SG90_MIN_ANGLE) angle = SG90_MIN_ANGLE;
        if (angle > SG90_MAX_ANGLE) angle = SG90_MAX_ANGLE;
        
        status = SG90_SetAngle(angle);
    }
    
    NAPI_CALL(env, napi_create_int32(env, status, &result));
    return result;
}

// 打开水泵
static napi_value pumpOn(napi_env env, napi_callback_info info)
{
    napi_value result;
    int status = pump_on();
    
    NAPI_CALL(env, napi_create_int32(env, status, &result));
    return result;
}

// 关闭水泵
static napi_value pumpOff(napi_env env, napi_callback_info info)
{
    napi_value result;
    int status = pump_off();
    
    NAPI_CALL(env, napi_create_int32(env, status, &result));
    return result;
}

// 读取光敏传感器数值
static napi_value readLightSensor(napi_env env, napi_callback_info info)
{
    napi_value result;
    int value = 0;
    
    if (light_sensor_read(&value) == 0) {
        NAPI_CALL(env, napi_create_int32(env, value, &result));
    } else {
        NAPI_CALL(env, napi_get_null(env, &result));
    }
    
    return result;
}

// 打开LED灯
static napi_value ledOn(napi_env env, napi_callback_info info)
{
    napi_value result;
    int status = LedOn();
    
    NAPI_CALL(env, napi_create_int32(env, status, &result));
    return result;
}

// 关闭LED灯
static napi_value ledOff(napi_env env, napi_callback_info info)
{
    napi_value result;
    int status = LedOff();
    
    NAPI_CALL(env, napi_create_int32(env, status, &result));
    return result;
}

// 统一控制风扇方向和速度
static napi_value controlFan(napi_env env, napi_callback_info info)
{
    napi_value result;
    size_t argc = 2;
    napi_value args[2];
    int direction = 0;
    int speed = 80;  // 默认速度为80%
    int status = -1;
    
    // 获取参数
    NAPI_CALL(env, napi_get_cb_info(env, info, &argc, args, nullptr, nullptr));
    
    // 获取方向参数
    if (argc >= 1) {
        NAPI_CALL(env, napi_get_value_int32(env, args[0], &direction));
    }
    
    // 获取速度参数（如果提供）
    if (argc >= 2) {
        NAPI_CALL(env, napi_get_value_int32(env, args[1], &speed));
        // 确保速度在有效范围内 0-100%
        if (speed < 0) speed = 0;
        if (speed > 100) speed = 100;
    }
    
    // 根据方向参数控制电机
    switch (direction) {
        case 0:  // 停止
            status = setMotorDirection(MOTOR_STOP);
            break;
        case 1:  // 正转
            status = controlMotor(MOTOR_FORWARD, speed);
            break;
        case 2:  // 反转
            status = controlMotor(MOTOR_BACKWARD, speed);
            break;
        default:
            status = setMotorDirection(MOTOR_STOP);
            break;
    }
    
    NAPI_CALL(env, napi_create_int32(env, status, &result));
    return result;
}

// 打开蜂鸣器
static napi_value buzzeron(napi_env env, napi_callback_info info)
{
    napi_value result;
    int status = BuzzerControl(1);
    
    NAPI_CALL(env, napi_create_int32(env, status, &result));
    return result;
}

// 关闭蜂鸣器
static napi_value buzzeroff(napi_env env, napi_callback_info info)
{
    napi_value result;
    int status = BuzzerControl(0);
    
    NAPI_CALL(env, napi_create_int32(env, status, &result));
    return result;
}

// 发送拍照指令到串口
static napi_value sendCapture(napi_env env, napi_callback_info info)
{
    napi_value result;
    int status = 0;
    
    // 发送字符串命令 "CAPTURE"
    const char* cmd = "CAPTURE";
    
    // 发送到串口
    write_uart(cmd, strlen(cmd));
    
    NAPI_CALL(env, napi_create_int32(env, status, &result));
    return result;
}

// 根据关键字获取数据
static napi_value getDataByKey(napi_env env, napi_callback_info info)
{
    napi_value result;
    size_t argc = 1;
    napi_value args[1];
    char key[64] = {0};
    size_t key_len = 0;
    float value = 0;
    
    // 获取参数
    NAPI_CALL(env, napi_get_cb_info(env, info, &argc, args, nullptr, nullptr));
    
    // 检查参数
    if (argc >= 1) {
        // 获取字符串参数
        NAPI_CALL(env, napi_get_value_string_utf8(env, args[0], key, sizeof(key) - 1, &key_len));
        
        // 通过关键字获取数据
        value = get_data_by_key(key);
        
        // 创建返回值
        NAPI_CALL(env, napi_create_double(env, value, &result));
        return result;
    }
    
    // 如果没有参数，返回0
    NAPI_CALL(env, napi_create_double(env, 0, &result));
    return result;
}

// 配置LLaMA服务器连接参数
static napi_value configLlama(napi_env env, napi_callback_info info)
{
    napi_value result;
    size_t argc = 5;  // 增加到5个参数，包括systemMessage、temperature和maxTokens
    napi_value args[5];
    char host[256] = "192.168.31.5"; // 默认主机
    size_t host_len = 0;
    int port = 8080; // 默认端口
    float temperature = 0.8f; // 默认温度
    int max_tokens = 1024; // 默认最大tokens
    char systemMessage[4096] = "你是一个非常有用的助手"; // 默认system消息
    size_t systemMessage_len = 0;
    int status = 0;
    
    // 获取参数
    NAPI_CALL(env, napi_get_cb_info(env, info, &argc, args, nullptr, nullptr));
    
    // 获取主机名
    if (argc >= 1) {
        // 确保第一个参数是字符串类型
        napi_valuetype valuetype;
        NAPI_CALL(env, napi_typeof(env, args[0], &valuetype));
        if (valuetype == napi_string) {
            NAPI_CALL(env, napi_get_value_string_utf8(env, args[0], host, sizeof(host) - 1, &host_len));
        }
    }
    
    // 获取端口号
    if (argc >= 2) {
        // 确保第二个参数是数字类型
        napi_valuetype valuetype;
        NAPI_CALL(env, napi_typeof(env, args[1], &valuetype));
        if (valuetype == napi_number) {
            NAPI_CALL(env, napi_get_value_int32(env, args[1], &port));
        }
    }
    
    // 获取systemMessage
    if (argc >= 3) {
        napi_valuetype valuetype;
        NAPI_CALL(env, napi_typeof(env, args[2], &valuetype));
        if (valuetype == napi_string) {
            NAPI_CALL(env, napi_get_value_string_utf8(env, args[2], systemMessage, sizeof(systemMessage) - 1, &systemMessage_len));
        }
    }
    
    // 获取temperature
    if (argc >= 4) {
        napi_valuetype valuetype;
        NAPI_CALL(env, napi_typeof(env, args[3], &valuetype));
        if (valuetype == napi_number) {
            double temp_value;
            NAPI_CALL(env, napi_get_value_double(env, args[3], &temp_value));
            temperature = static_cast<float>(temp_value);
            // 确保温度在有效范围内
            if (temperature < 0.0f) temperature = 0.0f;
            if (temperature > 1.0f) temperature = 1.0f;
        }
    }
    
    // 获取maxTokens
    if (argc >= 5) {
        napi_valuetype valuetype;
        NAPI_CALL(env, napi_typeof(env, args[4], &valuetype));
        if (valuetype == napi_number) {
            NAPI_CALL(env, napi_get_value_int32(env, args[4], &max_tokens));
            // 确保max_tokens是正数
            if (max_tokens <= 0) max_tokens = 1024;
        }
    }
    
    // 创建或更新LlamaClient实例
    if (g_llamaClient != nullptr) {
        delete g_llamaClient;
    }
    
    // 创建新的LlamaClient实例，传入所有配置参数
    g_llamaClient = new llama::LlamaClient(host, port, systemMessage, temperature, max_tokens);
    if (g_llamaClient == nullptr) {
        status = -1;
    } else {
        // 尝试连接到服务器
        if (!g_llamaClient->connect()) {
            status = -2;
        }
    }
    
    NAPI_CALL(env, napi_create_int32(env, status, &result));
    return result;
}

// 异步操作上下文结构体
struct AskLlamaContext {
    napi_async_work work;
    napi_deferred deferred;
    std::string prompt;
    std::string response;
    bool success;
};

// 向LLaMA发送请求的异步执行函数
static void AskLlamaExecute(napi_env env, void* data)
{
    AskLlamaContext* context = static_cast<AskLlamaContext*>(data);
    
    // 确保LlamaClient实例已创建
    if (g_llamaClient == nullptr) {
        g_llamaClient = new llama::LlamaClient();
        if (g_llamaClient == nullptr) {
            context->success = false;
            context->response = "Failed to create LlamaClient instance";
            return;
        }
    }
    
    // 如果未连接则尝试连接
    if (!g_llamaClient->isConnected()) {
        if (!g_llamaClient->connect()) {
            context->success = false;
            context->response = "Failed to connect to LLaMA server: " + g_llamaClient->getLastError();
            return;
        }
    }
    
    // 设置请求参数 - 不再设置temperature和max_tokens，因为已在client初始化时配置
    llama::LlamaRequestParams params;
    params.prompt = context->prompt;
    params.stream = false;  // 使用同步模式
    
    // 发送请求
    llama::LlamaResponse response = g_llamaClient->sendRequestSync(params);
    
    if (!response.error.empty()) {
        context->success = false;
        context->response = "LLaMA error: " + response.error;
        return;
    }
    
    context->success = true;
    context->response = response.text;
}

// 异步操作完成回调
static void AskLlamaComplete(napi_env env, napi_status status, void* data)
{
    AskLlamaContext* context = static_cast<AskLlamaContext*>(data);
    napi_value result;
    
    if (context->success) {
        // 创建响应字符串
        NAPI_CALL_RETURN_VOID(env, napi_create_string_utf8(env, context->response.c_str(), 
                                                        context->response.length(), &result));
        // 解决Promise并返回结果
        NAPI_CALL_RETURN_VOID(env, napi_resolve_deferred(env, context->deferred, result));
    } else {
        // 创建错误对象
        napi_value error, error_message;
        NAPI_CALL_RETURN_VOID(env, napi_create_string_utf8(env, context->response.c_str(), 
                                                        context->response.length(), &error_message));
        NAPI_CALL_RETURN_VOID(env, napi_create_error(env, nullptr, error_message, &error));
        // 拒绝Promise
        NAPI_CALL_RETURN_VOID(env, napi_reject_deferred(env, context->deferred, error));
    }
    
    // 删除异步工作和上下文
    NAPI_CALL_RETURN_VOID(env, napi_delete_async_work(env, context->work));
    delete context;
}

// 向LLaMA服务器发送请求并获取回复
static napi_value askLlama(napi_env env, napi_callback_info info)
{
    size_t argc = 1;  // 只接收prompt参数
    napi_value args[1];
    napi_value promise;
    char prompt[4096] = {0};
    size_t prompt_len = 0;
    
    // 获取参数
    NAPI_CALL(env, napi_get_cb_info(env, info, &argc, args, nullptr, nullptr));
    
    // 获取prompt
    if (argc >= 1) {
        NAPI_CALL(env, napi_get_value_string_utf8(env, args[0], prompt, sizeof(prompt) - 1, &prompt_len));
    }
    
    // 创建异步上下文
    AskLlamaContext* context = new AskLlamaContext();
    context->prompt = prompt;
    context->success = false;
    
    // 创建Promise
    NAPI_CALL(env, napi_create_promise(env, &context->deferred, &promise));
    
    // 创建异步工作
    napi_value resource_name;
    NAPI_CALL(env, napi_create_string_utf8(env, "AskLlama", NAPI_AUTO_LENGTH, &resource_name));
    
    NAPI_CALL(env, napi_create_async_work(
        env, nullptr, resource_name,
        AskLlamaExecute, AskLlamaComplete, 
        context, &context->work));
    
    // 将工作排入队列
    NAPI_CALL(env, napi_queue_async_work(env, context->work));
    
    return promise;
}

// 清除LLaMA消息历史
static napi_value clearLlamaHistory(napi_env env, napi_callback_info info)
{
    napi_value result;
    int status = 0;
    
    // 确保LlamaClient实例已创建
    if (g_llamaClient == nullptr) {
        status = -1; // 没有初始化LlamaClient
    } else {
        // 调用清除历史的方法
        g_llamaClient->clearHistory();
    }
    
    NAPI_CALL(env, napi_create_int32(env, status, &result));
    return result;
}

// 注册模块API
static napi_value register_myproject_apis(napi_env env, napi_value exports)
{
    napi_property_descriptor desc[] = {
        // 声明函数供ets调用
        DECLARE_NAPI_FUNCTION("initAllModules", initAllModules),
        DECLARE_NAPI_FUNCTION("ReadSoilMoistureRaw", ReadSoilMoistureRaw),
        DECLARE_NAPI_FUNCTION("setSG90Angle", setSG90Angle),
        DECLARE_NAPI_FUNCTION("pumpOn", pumpOn),
        DECLARE_NAPI_FUNCTION("pumpOff", pumpOff),
        DECLARE_NAPI_FUNCTION("readLightSensor", readLightSensor),
        DECLARE_NAPI_FUNCTION("ledOn", ledOn),
        DECLARE_NAPI_FUNCTION("ledOff", ledOff),
        DECLARE_NAPI_FUNCTION("controlFan", controlFan),
        DECLARE_NAPI_FUNCTION("buzzeron", buzzeron),
        DECLARE_NAPI_FUNCTION("buzzeroff", buzzeroff),
        DECLARE_NAPI_FUNCTION("sendCapture", sendCapture),
        DECLARE_NAPI_FUNCTION("getDataByKey", getDataByKey),
        // 添加LlamaClient相关函数
        DECLARE_NAPI_FUNCTION("configLlama", configLlama),
        DECLARE_NAPI_FUNCTION("askLlama", askLlama),
        DECLARE_NAPI_FUNCTION("clearLlamaHistory", clearLlamaHistory)
    };
    
    NAPI_CALL(env, napi_define_properties(env, exports, sizeof(desc) / sizeof(desc[0]), desc));
    return exports;
}

// 模块定义
static napi_module myproject_module = {
    .nm_version = 1,
    .nm_flags = 0,
    .nm_filename = nullptr,
    .nm_register_func = register_myproject_apis,
    .nm_modname = "myproject",  // 模块名
    .nm_priv = ((void *)0),
    .reserved = {0},
};

// 注册模块
extern "C" __attribute__((constructor)) void register_myproject_module(void)
{
    napi_module_register(&myproject_module);
}
