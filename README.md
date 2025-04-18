# 项目功能模块说明文档

本文档总结了项目中各个模块的功能、API接口以及相关宏定义，便于开发人员快速了解和使用。

## 目录
- [土壤湿度传感器](#土壤湿度传感器)
- [SG90舵机控制](#sg90舵机控制)
- [水泵控制](#水泵控制)
- [串口通信](#串口通信)
- [光敏传感器](#光敏传感器)
- [LED控制](#led控制)
- [风扇控制](#风扇控制)
- [蜂鸣器控制](#蜂鸣器控制)

## 土壤湿度传感器

**头文件**: `soil_moisture.h`

### 宏定义

土壤湿度状态定义:
- `SOIL_DRY`: 0 - 土壤干燥
- `SOIL_MODERATE`: 1 - 土壤湿度适中
- `SOIL_WET`: 2 - 土壤湿润

土壤湿度阈值定义:
- `DRY_THRESHOLD`: 1000 - 干燥阈值
- `WET_THRESHOLD`: 2500 - 湿润阈值

错误码定义:
- `SOIL_MOISTURE_OK`: 0 - 操作成功
- `SOIL_MOISTURE_ERROR`: -1 - 一般错误
- `SOIL_MOISTURE_ADC_ERR`: -2 - ADC读取错误

### 函数

```c
int soil_moisture_init(void);
```
初始化土壤湿度传感器。成功返回`SOIL_MOISTURE_OK`，其他值表示失败。

```c
int soil_moisture_read_raw(int *value);
```
读取土壤湿度原始ADC值。成功返回`SOIL_MOISTURE_OK`，其他值表示失败。

```c
int soil_moisture_read_percentage(int *percentage);
```
获取土壤湿度百分比(0-100)。成功返回`SOIL_MOISTURE_OK`，其他值表示失败。

```c
int soil_moisture_read_status(int *status);
```
获取土壤湿度状态(`SOIL_DRY`/`SOIL_MODERATE`/`SOIL_WET`)。成功返回`SOIL_MOISTURE_OK`，其他值表示失败。

## SG90舵机控制

**头文件**: `sg90.h`

### 宏定义

舵机角度范围:
- `SG90_MIN_ANGLE`: 0 - 最小角度
- `SG90_MAX_ANGLE`: 180 - 最大角度

舵机PWM控制参数(单位:纳秒):
- `SG90_PWM_PERIOD`: 20000000 - 20ms周期
- `SG90_MIN_PULSE`: 500000 - 0.5ms脉冲宽度(0度)
- `SG90_MAX_PULSE`: 2500000 - 2.5ms脉冲宽度(180度)

### 函数

```c
int SG90_Init(void);
```
初始化SG90舵机。成功返回>=0，失败返回<0。

```c
int SG90_SetAngle(int angle);
```
设置SG90舵机角度(0-180)。成功返回>=0，失败返回<0。

```c
int SG90_Close(void);
```
关闭SG90舵机控制(禁用PWM)。成功返回>=0，失败返回<0。

## 水泵控制

**头文件**: `pump_control.h`

### 函数

```c
int pump_init(void);
```
初始化水泵控制。成功返回0，失败返回负值。

```c
int pump_on(void);
```
打开水泵。成功返回0，失败返回负值。

```c
int pump_off(void);
```
关闭水泵。成功返回0，失败返回负值。

```c
int pump_get_status(int *status);
```
获取水泵状态。`status`为输出参数，1表示开启，0表示关闭。成功返回0，失败返回负值。

```c
int pump_deinit(void);
```
释放水泵控制资源。成功返回0，失败返回负值。

## 串口通信

**头文件**: `myserial.h`

### 宏定义

- `UART_TTL_NAME`: "/dev/ttyS1" - 串口设备名
- `MAX_BUFFER_SIZE`: 1024 - 最大缓冲区大小
- `FRAME_HEAD`: 0xFE - 帧头
- `FRAME_END`: 0xFF - 帧尾
- `ESC`: 0x7E - 转义字符
- `CAMERA_END`: 0x01 - 相机标识

### 函数

```c
void init_uart();
```
初始化UART设备并启动读线程。

```c
void write_uart(const char* buf, int len);
```
通过UART发送数据。目前只有发送“CAPTURE”控制副板拍照的功能

```c
unsigned char* return_recv(int* len);
```
返回接收到的数据。`len`为接收数据长度指针，返回值为接收数据缓冲区。
**注意**: 调用者负责释放返回的内存。

```c
float get_data_by_key(char *key);
```
解析数据并获取指定键的值。

## 光敏传感器

**头文件**: `light_sensor.h`

### 函数

```c
int light_sensor_init(void);
```
初始化光敏电阻ADC。成功返回0，失败返回-1。

```c
int light_sensor_read(int *value);
```
读取光敏电阻的值。成功返回0，失败返回-1。

```c
float light_sensor_to_percentage(int adc_value);
```
将ADC数值转换为光照强度百分比(0-100)。

## LED控制

**头文件**: `led_control.h`

### 宏定义

LED控制状态码:
- `LED_OK`: 0 - 成功
- `LED_ERROR`: -1 - 失败

LED状态:
- `LED_OFF`: 0 - LED关闭
- `LED_ON`: 1 - LED打开

GPIO配置:
- `LED_GPIO`: 381 - LED使用的GPIO引脚

### 函数

```c
int LedInit(void);
```
初始化LED。成功返回`LED_OK`，失败返回`LED_ERROR`。

```c
int LedOn(void);
```
打开LED。成功返回`LED_OK`，失败返回`LED_ERROR`。

```c
int LedOff(void);
```
关闭LED。成功返回`LED_OK`，失败返回`LED_ERROR`。

```c
int LedGetStatus(int *status);
```
获取LED当前状态。`status`为输出参数，返回`LED_ON`或`LED_OFF`。成功返回`LED_OK`，失败返回`LED_ERROR`.

```c
int LedToggle(void);
```
翻转LED状态。成功返回`LED_OK`，失败返回`LED_ERROR`.

```c
int LedDeinit(void);
```
释放LED资源。成功返回`LED_OK`，失败返回`LED_ERROR`.

## 风扇控制

**头文件**: `fan_control.h`

### 宏定义

电机引脚定义:
- `MOTOR_IN1_PIN`: 383 - GPIO383 用于控制方向
- `MOTOR_IN2_PIN`: 384 - GPIO384 用于控制方向
- `MOTOR_PWM_CHANNEL`: 2 - PWM2 用于控制速度

电机方向和输入信号对应关系:

| IN1 | IN2 | 电机状态 |
|-----|-----|----------|
| 0   | 0   | 停止     |
| 0   | 1   | 正转     |
| 1   | 0   | 反转     |

电机方向定义:
- `MOTOR_FORWARD`: 0 - 电机正转
- `MOTOR_BACKWARD`: 1 - 电机反转
- `MOTOR_STOP`: 2 - 电机停止
- `MOTOR_BRAKE`: 3 - 电机刹车

### 函数

```c
int initMotorControl();
```
初始化TB6612电机控制器。成功返回0，失败返回负值。

```c
int setMotorSpeed(uint8_t speed);
```
设置电机速度，范围0-100。成功返回0，失败返回负值。

```c
int setMotorDirection(MotorDirection direction);
```
设置电机旋转方向。direction可为`MOTOR_FORWARD`、`MOTOR_BACKWARD`、`MOTOR_STOP`或`MOTOR_BRAKE`。成功返回0，失败返回负值。

```c
int controlMotor(MotorDirection direction, uint8_t speed);
```
控制电机的方向和速度。成功返回0，失败返回负值。

```c
int stopMotor();
```
停止电机。成功返回0，失败返回负值。

## 蜂鸣器控制

**头文件**: `buzzer_control.h`

### 函数

```c
int BuzzerInit(void);
```
初始化蜂鸣器。成功返回0，失败返回负值。

```c
int BuzzerControl(int on);
```
蜂鸣器鸣响。`on`为1表示开启，0表示关闭。成功返回0，失败返回负值。

```c
int BuzzerBeep(int milliseconds);
```
蜂鸣器鸣响一段时间(毫秒)。成功返回0，失败返回负值。

```c
int BuzzerDeinit(void);
```
释放蜂鸣器资源。成功返回0，失败返回负值。