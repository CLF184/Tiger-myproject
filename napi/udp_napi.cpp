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

#include <cstring>

#include "napi/native_api.h"
#include "napi/native_common.h"
#include "napi/native_node_api.h"

#include "image_capture_callback_manager.h"
#include "sensor_data_provider.h"

namespace {
const char *kCaptureCmd = "CAPTURE";
}

/**
 * @brief 通过 UDP 广播发送 capture 命令
 * 
 * @param env Node-API 环境
 * @param info 回调信息
 * @return napi_value 返回状态码
 */
static napi_value sendCapture(napi_env env, napi_callback_info info)
{
    (void)info;
    napi_value result;
    int status = sensor::SendCommand(kCaptureCmd);
    
    NAPI_CALL(env, napi_create_int32(env, status, &result));
    return result;
}

/**
 * @brief 从 UDP 接收到的数据中根据键名获取对应的值
 * 
 * @param env Node-API 环境
 * @param info 回调信息，包含参数
 * @return napi_value 返回浮点数值
 */
static napi_value getDataByKey(napi_env env, napi_callback_info info)
{
    napi_value result;
    size_t argc = 1;
    napi_value args[1];
    char key[64] = {0};
    size_t key_len = 0;
    float value = 0;

    NAPI_CALL(env, napi_get_cb_info(env, info, &argc, args, nullptr, nullptr));

    if (argc >= 1) {
        NAPI_CALL(env, napi_get_value_string_utf8(env, args[0], key, sizeof(key) - 1, &key_len));
        value = sensor::GetDataByKey(key);

        NAPI_CALL(env, napi_create_double(env, value, &result));
        return result;
    }

    NAPI_CALL(env, napi_create_double(env, 0, &result));
    return result;
}

/**
 * @brief 注册图片捕获回调函数
 * 
 * @param env Node-API 环境
 * @param info 回调信息，包含回调函数参数
 * @return napi_value 返回状态码
 */
static napi_value onImageCaptured(napi_env env, napi_callback_info info)
{
    return ImageCaptureOn(env, info);
}

/**
 * @brief 取消图片捕获回调函数
 * 
 * @param env Node-API 环境
 * @param info 回调信息
 * @return napi_value 返回状态码
 */
static napi_value offImageCaptured(napi_env env, napi_callback_info info)
{
    return ImageCaptureOff(env, info);
}

/**
 * @brief 注册 UDP 相关的 NAPI 接口
 * 
 * @param env Node-API 环境
 * @param exports 导出对象
 * @return napi_value 返回导出对象
 */
napi_value RegisterUdpApis(napi_env env, napi_value exports)
{
    sensor::SetDataChannel(sensor::DataChannel::UDP);

    napi_property_descriptor desc[] = {
        DECLARE_NAPI_FUNCTION("sendCapture", sendCapture),
        DECLARE_NAPI_FUNCTION("getDataByKey", getDataByKey),
        DECLARE_NAPI_FUNCTION("onImageCaptured", onImageCaptured),
        DECLARE_NAPI_FUNCTION("offImageCaptured", offImageCaptured),
    };
    NAPI_CALL(env, napi_define_properties(env, exports, sizeof(desc) / sizeof(desc[0]), desc));
    return exports;
}
