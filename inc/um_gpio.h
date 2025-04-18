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

#ifndef __UM_GPIO_H__
#define __UM_GPIO_H__

#ifdef __cplusplus
extern "C" {
#endif

#define UM_GPIO_EXPORT "/sys/class/gpio/export"
#define UM_GPIO_UNEXPORT "/sys/class/gpio/unexport"
#define UM_GPIO_PEX "/sys/class/gpio/gpio"

// hilog
#undef LOG_DOMAIN
#undef LOG_TAG
#define LOG_DOMAIN 0  // 标识业务领域，范围0x0~0xFFFFF
#define LOG_TAG "GPIO_TEST"

// gpios
#define UM_GPIO_01 380 /* GPIO.7 */
#define UM_GPIO_02 381 /* GPIO.0 */
#define UM_GPIO_03 382 /* GPIO.2 */
#define UM_GPIO_04 383 /* GPIO.3 */
#define UM_GPIO_05 384 /* GPIO.21 */
#define UM_GPIO_06 385 /* GPIO.22 */
#define UM_GPIO_07 386 /* GPIO.23 */
#define UM_GPIO_08 387 /* GPIO.24 */
#define UM_GPIO_09 388 /* GPIO.25 */
#define UM_GPIO_10 389 /* GPIO.1 */
#define UM_GPIO_11 390 /* GPIO.4 */
#define UM_GPIO_12 391 /* GPIO.5 */
#define UM_GPIO_13 392 /* GPIO.6 */
#define UM_GPIO_14 393 /* GPIO.30 */
#define UM_GPIO_15 394 /* GPIO.26 */
#define UM_GPIO_16 395 /* GPIO.27 */

// spi cs
#define UM_GPIO_SPI_CS 486 /* SS0 */

// direction
#define UM_GPIO_DIRECTION_IN 0
#define UM_GPIO_DIRECTION_OUT 1

// is export
#define UM_GPIO_NOT_EXPORT 0
#define UM_GPIO_EXPORTED 1

// errno
#define UM_GPIO_ERR (-1)
#define UM_GPIO_NOT_EXPROT_ERROR (-2)

// value high - low level
#define UM_GPIO_LOW_LEVE 0
#define UM_GPIO_HIGH_LEVE 1

/**
 * set gpio export
 * @param gpioNum gpioNum
 * @param bExport export,0:not export 1:export
 */
int UM_GPIO_Export(int gpioNum, int bExport);

/**
 * set gpio direction
 * @param gpioNum gpioNum
 * @param direction direction,0:in 1:out
 */
int UM_GPIO_SetDirection(int gpioNum, int direction);

/**
 * set gpio value
 * @param gpioNum gpioNum
 * @param value value,0:low 1:high
 */
int UM_GPIO_SetValue(int gpioNum, int value);

/**
 * check gpio export or not
 * @param gpioNum gpioNum
 * @param *value export,0:not export 1:exported
 */
int UM_GPIO_IsExport(int gpioNum, int *value);

/**
 * get gpio direction
 * @param gpioNum gpioNum
 * @param *value direction,0:in 1:out
 */
int UM_GPIO_GetDirection(int gpioNum, int *value);

/**
 * get gpio value
 * @param gpioNum gpioNum
 * @param *value value,0:low 1:high
 */
int UM_GPIO_GetValue(int gpioNum, int *value);

#ifdef __cplusplus
}
#endif
#endif /* __UM_GPIO_H__ */