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
#include <cstdint>

#include "napi/native_api.h"
#include "napi/native_common.h"
#include "napi/native_node_api.h"

#include "napi/myproject_napi_register.h"

// 仅保留初始化相关能力：其余 NAPI 接口已拆分到 napi/ 目录
#include "buzzer_control.h"
#include "fan_control.h"
#include "led_control.h"
#include "light_sensor.h"
#include "myserial.h"
#include "wifi_udp_receiver.h"
#include "pump_control.h"
#include "sg90.h"
#include "soil_moisture.h"

#include "auto_control.h"
#include "mqtt_payload_builder.h"
#include "device_identity.h"

static napi_value initAllModules(napi_env env, napi_callback_info info)
{
    napi_value result;
    uint32_t failMask = 0;

    enum InitFailBits : uint32_t {
        INIT_FAIL_SG90 = 1u << 0,
        INIT_FAIL_PUMP = 1u << 1,
        INIT_FAIL_LIGHT_SENSOR = 1u << 2,
        INIT_FAIL_LED = 1u << 3,
        INIT_FAIL_FAN = 1u << 4,
        INIT_FAIL_BUZZER = 1u << 5,
    };

    auto recordFail = [&](uint32_t bit, int ret) {
        if (ret != 0) {
            failMask |= bit;
        }
    };

    // 土壤湿度外设已更换：土壤湿度从串口数据键 SoilHumi 获取，旧 ADC 模块初始化失败不应阻塞整体初始化
    (void)soil_moisture_init();
    recordFail(INIT_FAIL_SG90, SG90_Init());
    recordFail(INIT_FAIL_PUMP, pump_init());
    recordFail(INIT_FAIL_LIGHT_SENSOR, light_sensor_init());
    recordFail(INIT_FAIL_LED, LedInit());
    recordFail(INIT_FAIL_FAN, initMotorControl());
    recordFail(INIT_FAIL_BUZZER, BuzzerInit());

    // 串口用于相机/控制命令，传感器数据改为 WiFi UDP 通道
    init_uart();
    init_wifi_udp_receiver();
    
        // 提前初始化 deviceId（芯片/板级唯一 ID），用于 MQTT payload/topic 与自动控制命令 topic。
        mqttc::SetMqttPayloadDeviceId(device_identity::GetBestEffortChipId());

    // 自动控制线程：默认阈值不生效（由全局开关决定），线程常驻用于接收阈值/命令
    control::Start();

    NAPI_CALL(env, napi_create_uint32(env, failMask, &result));
    return result;
}

// 注册模块API
static napi_value register_myproject_apis(napi_env env, napi_value exports)
{
    napi_property_descriptor desc[] = {
        // 声明函数供ets调用
        DECLARE_NAPI_FUNCTION("initAllModules", initAllModules),
    };

    NAPI_CALL(env, napi_define_properties(env, exports, sizeof(desc) / sizeof(desc[0]), desc));

    // 其余模块接口分散注册到同一个 exports 上
    RegisterSoilMoistureApis(env, exports);
    RegisterSg90Apis(env, exports);
    RegisterPumpApis(env, exports);
    RegisterLightSensorApis(env, exports);
    RegisterLedApis(env, exports);
    RegisterFanApis(env, exports);
    RegisterBuzzerApis(env, exports);
    RegisterSerialApis(env, exports);
    RegisterLlamaApis(env, exports);
    RegisterMqttApis(env, exports);
    RegisterControlApis(env, exports);
    return exports;
}

// 模块定义
static napi_module myproject_module = {
    .nm_version = 1,
    .nm_flags = 0,
    .nm_filename = nullptr,
    .nm_register_func = register_myproject_apis,
    .nm_modname = "myproject", // 模块名
    .nm_priv = ((void *)0),
    .reserved = {0},
};

// 注册模块
extern "C" __attribute__((constructor)) void register_myproject_module(void)
{
    napi_module_register(&myproject_module);
}
