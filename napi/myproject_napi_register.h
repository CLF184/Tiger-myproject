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

#ifndef MYPROJECT_NAPI_REGISTER_H
#define MYPROJECT_NAPI_REGISTER_H

#include "napi/native_api.h"

// 每个模块把自己的 NAPI 接口注册到统一 exports 上
napi_value RegisterBuzzerApis(napi_env env, napi_value exports);
napi_value RegisterFanApis(napi_env env, napi_value exports);
napi_value RegisterLedApis(napi_env env, napi_value exports);
napi_value RegisterLightSensorApis(napi_env env, napi_value exports);
napi_value RegisterPumpApis(napi_env env, napi_value exports);
napi_value RegisterSerialApis(napi_env env, napi_value exports);
napi_value RegisterSg90Apis(napi_env env, napi_value exports);
napi_value RegisterSoilMoistureApis(napi_env env, napi_value exports);
napi_value RegisterLlamaApis(napi_env env, napi_value exports);
napi_value RegisterMqttApis(napi_env env, napi_value exports);
napi_value RegisterControlApis(napi_env env, napi_value exports);

#endif
