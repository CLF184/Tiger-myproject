#pragma once

#include <stdint.h>

namespace camera_module {

using transmit_fn_t = void (*)(const char* buf, int len, unsigned char type);

constexpr unsigned char CAMERA_END = 0x01; // 相机帧结束标记

void set_transmit(transmit_fn_t fn);
void init();

// 供命令分发直接调用（签名与现有 handlers 兼容）
void capture_command(const char* command);
void set_frame_size_command(const char* command);

} // namespace camera_module
