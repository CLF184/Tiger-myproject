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

#include "napi/native_api.h"
#include "napi/native_common.h"
#include "napi/native_node_api.h"

#include "pump_control.h"

static napi_value pumpOn(napi_env env, napi_callback_info info)
{
    napi_value result;
    int status = pump_on();
    NAPI_CALL(env, napi_create_int32(env, status, &result));
    return result;
}

static napi_value pumpOff(napi_env env, napi_callback_info info)
{
    napi_value result;
    int status = pump_off();
    NAPI_CALL(env, napi_create_int32(env, status, &result));
    return result;
}

napi_value RegisterPumpApis(napi_env env, napi_value exports)
{
    napi_property_descriptor desc[] = {
        DECLARE_NAPI_FUNCTION("pumpOn", pumpOn),
        DECLARE_NAPI_FUNCTION("pumpOff", pumpOff),
    };
    NAPI_CALL(env, napi_define_properties(env, exports, sizeof(desc) / sizeof(desc[0]), desc));
    return exports;
}
