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
#include "pump_control.h"
#include "sg90.h"
#include "soil_moisture.h"

static napi_value initAllModules(napi_env env, napi_callback_info info)
{
    napi_value result;
    int status = 0;
    int module_status = 0;

    module_status = soil_moisture_init();
    if (module_status != 0)
    {
        status = -1;
        goto finish;
    }

    module_status = SG90_Init();
    if (module_status != 0)
    {
        status = -2;
        goto finish;
    }

    module_status = pump_init();
    if (module_status != 0)
    {
        status = -3;
        goto finish;
    }

    module_status = light_sensor_init();
    if (module_status != 0)
    {
        status = -4;
        goto finish;
    }

    module_status = LedInit();
    if (module_status != 0)
    {
        status = -5;
        goto finish;
    }

    module_status = initMotorControl();
    if (module_status != 0)
    {
        status = -6;
        goto finish;
    }

    module_status = BuzzerInit();
    if (module_status != 0)
    {
        status = -7;
        goto finish;
    }

    init_uart();

finish:
    NAPI_CALL(env, napi_create_int32(env, status, &result));
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
