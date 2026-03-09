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

#ifndef IMAGE_CAPTURE_CALLBACK_MANAGER_H
#define IMAGE_CAPTURE_CALLBACK_MANAGER_H

#include "napi/native_api.h"

#ifdef __cplusplus
extern "C" {
#endif

// Native side entry used by serial/udp receive modules.
void NotifyImageCapturedFromNative(const char* path);

#ifdef __cplusplus
}
#endif

napi_value ImageCaptureOn(napi_env env, napi_callback_info info);
napi_value ImageCaptureOff(napi_env env, napi_callback_info info);

#endif // IMAGE_CAPTURE_CALLBACK_MANAGER_H
