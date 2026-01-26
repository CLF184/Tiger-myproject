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

#include "sg90.h"

static napi_value setSG90Angle(napi_env env, napi_callback_info info)
{
    napi_value result;
    size_t argc = 1;
    napi_value args[1];
    int angle = 0;
    int status = -1;

    NAPI_CALL(env, napi_get_cb_info(env, info, &argc, args, nullptr, nullptr));

    if (argc >= 1) {
        NAPI_CALL(env, napi_get_value_int32(env, args[0], &angle));
        if (angle < SG90_MIN_ANGLE) angle = SG90_MIN_ANGLE;
        if (angle > SG90_MAX_ANGLE) angle = SG90_MAX_ANGLE;
        status = SG90_SetAngle(angle);
    }

    NAPI_CALL(env, napi_create_int32(env, status, &result));
    return result;
}

napi_value RegisterSg90Apis(napi_env env, napi_value exports)
{
    napi_property_descriptor desc[] = {
        DECLARE_NAPI_FUNCTION("setSG90Angle", setSG90Angle),
    };
    NAPI_CALL(env, napi_define_properties(env, exports, sizeof(desc) / sizeof(desc[0]), desc));
    return exports;
}
