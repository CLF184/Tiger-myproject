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

#include "fan_control.h"

static napi_value controlFan(napi_env env, napi_callback_info info)
{
    napi_value result;
    size_t argc = 2;
    napi_value args[2];
    int direction = 0;
    int speed = 80;
    int status = -1;

    NAPI_CALL(env, napi_get_cb_info(env, info, &argc, args, nullptr, nullptr));

    if (argc >= 1) {
        NAPI_CALL(env, napi_get_value_int32(env, args[0], &direction));
    }
    if (argc >= 2) {
        NAPI_CALL(env, napi_get_value_int32(env, args[1], &speed));
        if (speed < 0) speed = 0;
        if (speed > 100) speed = 100;
    }

    switch (direction) {
        case 0:
            status = setMotorDirection(MOTOR_STOP);
            break;
        case 1:
            status = controlMotor(MOTOR_FORWARD, speed);
            break;
        case 2:
            status = controlMotor(MOTOR_BACKWARD, speed);
            break;
        default:
            status = setMotorDirection(MOTOR_STOP);
            break;
    }

    NAPI_CALL(env, napi_create_int32(env, status, &result));
    return result;
}

napi_value RegisterFanApis(napi_env env, napi_value exports)
{
    napi_property_descriptor desc[] = {
        DECLARE_NAPI_FUNCTION("controlFan", controlFan),
    };
    NAPI_CALL(env, napi_define_properties(env, exports, sizeof(desc) / sizeof(desc[0]), desc));
    return exports;
}
