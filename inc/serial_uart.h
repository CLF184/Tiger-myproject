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

#ifndef _SERIAL_UART_H_
#define _SERIAL_UART_H_

#ifdef __cplusplus
extern "C" {
#endif

//#define FRAME_LEN 4
#define OK 0
#define ERR (-1)

int uart_init(int fd, int uartBaud);

#ifdef __cplusplus
}
#endif

#endif
