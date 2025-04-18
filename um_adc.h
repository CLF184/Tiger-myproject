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

#ifndef __UM_ADC_H__
#define __UM_ADC_H__

#ifdef __cplusplus
extern "C" {
#endif

#define ADC_1 1
#define ADC_2 2
#define ADC_CHANNEL_1 "/sys/bus/iio/devices/iio:device0/in_voltage2_raw"
#define ADC_CHANNEL_2 "/sys/bus/iio/devices/iio:device0/in_voltage3_raw"
#define TEMP_CONST 0.042

#define ADC_OK 0
#define ADC_ERROR (-1)

int get_adc_data(int adc_channel, int *value);

#ifdef __cplusplus
}
#endif

#endif // endif __UM_ADC_H__
