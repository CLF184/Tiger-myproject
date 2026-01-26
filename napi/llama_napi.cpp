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

#include <string>

#include "napi/native_api.h"
#include "napi/native_common.h"
#include "napi/native_node_api.h"

#include "llama_client.h"

static llama::LlamaClient* g_llamaClient = nullptr;

static napi_value configLlama(napi_env env, napi_callback_info info)
{
    napi_value result;
    size_t argc = 5;
    napi_value args[5];
    char host[256] = "192.168.31.5";
    size_t host_len = 0;
    int port = 8080;
    float temperature = 0.8f;
    int max_tokens = 1024;
    char systemMessage[4096] = "你是一个非常有用的助手";
    size_t systemMessage_len = 0;
    int status = 0;

    NAPI_CALL(env, napi_get_cb_info(env, info, &argc, args, nullptr, nullptr));

    if (argc >= 1) {
        napi_valuetype valuetype;
        NAPI_CALL(env, napi_typeof(env, args[0], &valuetype));
        if (valuetype == napi_string) {
            NAPI_CALL(env, napi_get_value_string_utf8(env, args[0], host, sizeof(host) - 1, &host_len));
        }
    }

    if (argc >= 2) {
        napi_valuetype valuetype;
        NAPI_CALL(env, napi_typeof(env, args[1], &valuetype));
        if (valuetype == napi_number) {
            NAPI_CALL(env, napi_get_value_int32(env, args[1], &port));
        }
    }

    if (argc >= 3) {
        napi_valuetype valuetype;
        NAPI_CALL(env, napi_typeof(env, args[2], &valuetype));
        if (valuetype == napi_string) {
            NAPI_CALL(env, napi_get_value_string_utf8(env, args[2], systemMessage, sizeof(systemMessage) - 1, &systemMessage_len));
        }
    }

    if (argc >= 4) {
        napi_valuetype valuetype;
        NAPI_CALL(env, napi_typeof(env, args[3], &valuetype));
        if (valuetype == napi_number) {
            double temp_value;
            NAPI_CALL(env, napi_get_value_double(env, args[3], &temp_value));
            temperature = static_cast<float>(temp_value);
            if (temperature < 0.0f) temperature = 0.0f;
            if (temperature > 1.0f) temperature = 1.0f;
        }
    }

    if (argc >= 5) {
        napi_valuetype valuetype;
        NAPI_CALL(env, napi_typeof(env, args[4], &valuetype));
        if (valuetype == napi_number) {
            NAPI_CALL(env, napi_get_value_int32(env, args[4], &max_tokens));
            if (max_tokens <= 0) max_tokens = 1024;
        }
    }

    if (g_llamaClient != nullptr) {
        delete g_llamaClient;
        g_llamaClient = nullptr;
    }

    g_llamaClient = new llama::LlamaClient(host, port, systemMessage, temperature, max_tokens);
    if (g_llamaClient == nullptr) {
        status = -1;
    } else {
        if (!g_llamaClient->connect()) {
            status = -2;
        }
    }

    NAPI_CALL(env, napi_create_int32(env, status, &result));
    return result;
}

struct AskLlamaContext {
    napi_async_work work;
    napi_deferred deferred;
    std::string prompt;
    std::string response;
    bool success;
};

static void AskLlamaExecute(napi_env env, void* data)
{
    AskLlamaContext* context = static_cast<AskLlamaContext*>(data);

    if (g_llamaClient == nullptr) {
        g_llamaClient = new llama::LlamaClient();
        if (g_llamaClient == nullptr) {
            context->success = false;
            context->response = "Failed to create LlamaClient instance";
            return;
        }
    }

    if (!g_llamaClient->isConnected()) {
        if (!g_llamaClient->connect()) {
            context->success = false;
            context->response = "Failed to connect to LLaMA server: " + g_llamaClient->getLastError();
            return;
        }
    }

    llama::LlamaRequestParams params;
    params.prompt = context->prompt;
    params.stream = false;

    llama::LlamaResponse response = g_llamaClient->sendRequestSync(params);

    if (!response.error.empty()) {
        context->success = false;
        context->response = "LLaMA error: " + response.error;
        return;
    }

    context->success = true;
    context->response = response.text;
}

static void AskLlamaComplete(napi_env env, napi_status status, void* data)
{
    AskLlamaContext* context = static_cast<AskLlamaContext*>(data);
    napi_value result;

    if (context->success) {
        NAPI_CALL_RETURN_VOID(env, napi_create_string_utf8(env, context->response.c_str(),
            context->response.length(), &result));
        NAPI_CALL_RETURN_VOID(env, napi_resolve_deferred(env, context->deferred, result));
    } else {
        napi_value error;
        napi_value error_message;
        NAPI_CALL_RETURN_VOID(env, napi_create_string_utf8(env, context->response.c_str(),
            context->response.length(), &error_message));
        NAPI_CALL_RETURN_VOID(env, napi_create_error(env, nullptr, error_message, &error));
        NAPI_CALL_RETURN_VOID(env, napi_reject_deferred(env, context->deferred, error));
    }

    NAPI_CALL_RETURN_VOID(env, napi_delete_async_work(env, context->work));
    delete context;
}

static napi_value askLlama(napi_env env, napi_callback_info info)
{
    size_t argc = 1;
    napi_value args[1];
    napi_value promise;
    char prompt[4096] = {0};
    size_t prompt_len = 0;

    NAPI_CALL(env, napi_get_cb_info(env, info, &argc, args, nullptr, nullptr));

    if (argc >= 1) {
        NAPI_CALL(env, napi_get_value_string_utf8(env, args[0], prompt, sizeof(prompt) - 1, &prompt_len));
    }

    AskLlamaContext* context = new AskLlamaContext();
    context->prompt = prompt;
    context->success = false;

    NAPI_CALL(env, napi_create_promise(env, &context->deferred, &promise));

    napi_value resource_name;
    NAPI_CALL(env, napi_create_string_utf8(env, "AskLlama", NAPI_AUTO_LENGTH, &resource_name));

    NAPI_CALL(env, napi_create_async_work(
        env, nullptr, resource_name,
        AskLlamaExecute, AskLlamaComplete,
        context, &context->work));

    NAPI_CALL(env, napi_queue_async_work(env, context->work));
    return promise;
}

static napi_value clearLlamaHistory(napi_env env, napi_callback_info info)
{
    napi_value result;
    int status = 0;

    if (g_llamaClient == nullptr) {
        status = -1;
    } else {
        g_llamaClient->clearHistory();
    }

    NAPI_CALL(env, napi_create_int32(env, status, &result));
    return result;
}

napi_value RegisterLlamaApis(napi_env env, napi_value exports)
{
    napi_property_descriptor desc[] = {
        DECLARE_NAPI_FUNCTION("configLlama", configLlama),
        DECLARE_NAPI_FUNCTION("askLlama", askLlama),
        DECLARE_NAPI_FUNCTION("clearLlamaHistory", clearLlamaHistory),
    };
    NAPI_CALL(env, napi_define_properties(env, exports, sizeof(desc) / sizeof(desc[0]), desc));
    return exports;
}
