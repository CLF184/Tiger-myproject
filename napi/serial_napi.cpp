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
#include <new>
#include <pthread.h>
#include <string>

#include "napi/native_api.h"
#include "napi/native_common.h"
#include "napi/native_node_api.h"

#include "myserial.h"

namespace {
napi_threadsafe_function g_imageTsfn = nullptr;
pthread_mutex_t g_tsfnMutex = PTHREAD_MUTEX_INITIALIZER;

void CallJsOnImageCaptured(napi_env env, napi_value jsCb, void* /*context*/, void* data)
{
    std::string* path = static_cast<std::string*>(data);
    if (env == nullptr || jsCb == nullptr || path == nullptr) {
        delete path;
        return;
    }

    napi_value undefined;
    napi_get_undefined(env, &undefined);

    napi_value argv[1];
    napi_create_string_utf8(env, path->c_str(), path->size(), &argv[0]);
    napi_call_function(env, undefined, jsCb, 1, argv, nullptr);
    delete path;
}

void ReleaseImageTsfnLocked()
{
    if (g_imageTsfn != nullptr) {
        napi_release_threadsafe_function(g_imageTsfn, napi_tsfn_abort);
        g_imageTsfn = nullptr;
    }
}
} // namespace

extern "C" void NotifyImageCapturedFromNative(const char* path)
{
    if (path == nullptr) {
        return;
    }

    pthread_mutex_lock(&g_tsfnMutex);
    napi_threadsafe_function tsfn = g_imageTsfn;
    pthread_mutex_unlock(&g_tsfnMutex);

    if (tsfn == nullptr) {
        return;
    }

    auto* payload = new (std::nothrow) std::string(path);
    if (payload == nullptr) {
        return;
    }

    napi_status st = napi_call_threadsafe_function(tsfn, payload, napi_tsfn_nonblocking);
    if (st != napi_ok) {
        delete payload;
    }
}

static napi_value sendCapture(napi_env env, napi_callback_info info)
{
    napi_value result;
    int status = 0;
    const char* cmd = "CAPTURE";
    write_uart(cmd, strlen(cmd));
    NAPI_CALL(env, napi_create_int32(env, status, &result));
    return result;
}

static napi_value onImageCaptured(napi_env env, napi_callback_info info)
{
    napi_value result;
    size_t argc = 1;
    napi_value args[1];
    NAPI_CALL(env, napi_get_cb_info(env, info, &argc, args, nullptr, nullptr));

    if (argc < 1) {
        NAPI_CALL(env, napi_create_int32(env, -1, &result));
        return result;
    }

    napi_valuetype t;
    NAPI_CALL(env, napi_typeof(env, args[0], &t));
    if (t != napi_function) {
        NAPI_CALL(env, napi_create_int32(env, -2, &result));
        return result;
    }

    pthread_mutex_lock(&g_tsfnMutex);
    ReleaseImageTsfnLocked();

    napi_value resourceName;
    napi_create_string_utf8(env, "onImageCaptured", NAPI_AUTO_LENGTH, &resourceName);

    napi_status st = napi_create_threadsafe_function(
        env,
        args[0],
        nullptr,
        resourceName,
        0,
        1,
        nullptr,
        nullptr,
        nullptr,
        CallJsOnImageCaptured,
        &g_imageTsfn);
    pthread_mutex_unlock(&g_tsfnMutex);

    if (st != napi_ok) {
        NAPI_CALL(env, napi_create_int32(env, -3, &result));
        return result;
    }

    NAPI_CALL(env, napi_create_int32(env, 0, &result));
    return result;
}

static napi_value offImageCaptured(napi_env env, napi_callback_info /*info*/)
{
    napi_value result;
    pthread_mutex_lock(&g_tsfnMutex);
    ReleaseImageTsfnLocked();
    pthread_mutex_unlock(&g_tsfnMutex);

    NAPI_CALL(env, napi_create_int32(env, 0, &result));
    return result;
}

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
        value = get_data_by_key(key);
        NAPI_CALL(env, napi_create_double(env, value, &result));
        return result;
    }

    NAPI_CALL(env, napi_create_double(env, 0, &result));
    return result;
}

napi_value RegisterSerialApis(napi_env env, napi_value exports)
{
    napi_property_descriptor desc[] = {
        DECLARE_NAPI_FUNCTION("sendCapture", sendCapture),
        DECLARE_NAPI_FUNCTION("getDataByKey", getDataByKey),
        DECLARE_NAPI_FUNCTION("onImageCaptured", onImageCaptured),
        DECLARE_NAPI_FUNCTION("offImageCaptured", offImageCaptured),
    };
    NAPI_CALL(env, napi_define_properties(env, exports, sizeof(desc) / sizeof(desc[0]), desc));
    return exports;
}
