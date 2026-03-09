# 项目功能模块说明文档

本文档总结了项目中各个模块的功能、API接口以及相关宏定义，便于开发人员快速了解和使用。

## 项目结构

```c
/home/user/OH-Unionpi/
└── sub_sample/
    └── cpt_sample/
        └── myproject_napi/
            ├── BUILD.gn                     # 构建脚本
            ├── @ohos.myproject.d.ts         # TypeScript 类型声明
            ├── myproject_napi.cpp           # NAPI 主入口（聚合注册）
            ├── README.md
            ├── app/                         # 业务能力层（MQTT/LLaMA/串口/设备标识）
            │   ├── inc/
            │   └── src/
            ├── control/                     # 自动控制闭环逻辑
            │   ├── inc/
            │   └── src/
            ├── drivers/                     # 设备驱动层（泵/风扇/LED/蜂鸣器/传感器）
            │   ├── inc/
            │   └── src/
            ├── hal/                         # 硬件抽象层（GPIO/PWM/ADC/UART）
            │   ├── inc/
            │   └── src/
            ├── napi/                        # 各模块 NAPI 导出实现
            ├── ets/pages/                   # OpenHarmony ETS 页面
            ├── esp32_s3/                    # ESP32-S3 采集端工程（PlatformIO）
            ├── qt/                          # Qt 上位机
            └── third_party/                 # 第三方依赖（cJSON / MQTT-C）
```

### 分层说明（按当前代码）

- `myproject_napi.cpp`：负责 `initAllModules()` 和各 NAPI 子模块统一注册。
- `napi/*.cpp`：将驱动层/业务层能力映射为 ETS 可调用接口。
- `drivers/` + `hal/`：执行器与传感器驱动，以及底层硬件访问。
- `control/`：设备侧自动控制线程与阈值闭环控制。
- `app/`：MQTT 通信、LLaMA 客户端、串口与设备身份等通用业务能力。
- `esp32_s3/`：传感器采集与 UDP 广播固件代码。
- `ets/pages/` + `qt/`：前端页面与上位机侧联调入口。

## 目录
- [自动控制（设备侧闭环）](#自动控制设备侧闭环)
- [传感器数据获取](#传感器数据获取)
- [SG90舵机控制](#sg90舵机控制)
- [水泵控制](#水泵控制)
- [串口通信](#串口通信)
- [LED控制](#led控制)
- [风扇控制](#风扇控制)
- [蜂鸣器控制](#蜂鸣器控制)
- [LLaMA客户端](#llama客户端)

## 自动控制（设备侧闭环）

自动控制逻辑运行在设备侧 C/C++ 常驻线程中：线程启动后会周期性读取传感器值，并根据迟滞阈值控制执行器（泵/LED/风扇/蜂鸣器）。

### ETS/NAPI 接口（@ohos.myproject）

- `setAutoControlEnabled(enabled: boolean): number`
- `getAutoControlEnabled(): boolean`
- `setAutoControlThresholds(cfg: { soil_on?; soil_off?; light_on?; light_off?; temp_on?; temp_off?; ch2o_on?; ch2o_off?; co2_on?; co2_off?; co2_night_on?; co2_night_off?; ph_min?; ph_max?; ec_min?; ec_max?; n_min?; n_max?; p_min?; p_max?; k_min?; k_max?; fan_speed? }): number`
- `getAutoControlThresholds(): object`
- `setAutoControlCommandTopic(topic: string): number`（可选，覆盖默认命令 topic）

说明：
- `setAutoControlThresholds` 的字段均为可选；未提供的字段保持不变。
- 建议先 `setAutoControlThresholds` 再 `setAutoControlEnabled(true)`。

### Qt/MQTT 下发命令

### MQTT 设备自动发现（Retained Message）

为实现设备的自动发现与绑定，本系统基于 MQTT 保留消息机制设计了轻量级设备发现流程：
系统约定全局固定的设备发现主题前缀为 `<prefix>/announce/#`（默认 prefix 为 `ciallo_ohos`）；
开发板上线后，首先将自身的硬件唯一 ID（芯片/板级唯一 ID）、设备类型、数据/控制 topic 等信息封装为 JSON 消息，
以保留消息（Retained Message）的形式发布至 `<prefix>/announce/<deviceId>`；
Qt 跨平台客户端启动后，先订阅 `<prefix>/announce/#`，即可立即获取当前设备的唯一标识，
并自动完成数据/控制 topic 的拼接与绑定，无需人工干预。

说明：`<prefix>` 可通过前端的 Topic 输入框修改（例如将 `ciallo_ohos/sensors` 改为 `myproj/sensors`），
客户端/设备侧会随之前缀自动切换 announce/data/control 的命名空间。

推荐命令 topic（按设备隔离）：

```text
ciallo_ohos/<deviceId>/control
```

其中 `<deviceId>` 为设备侧芯片/板级唯一 ID（用于 discovery/payload/topic）。

兼容模式（公共控制通道）：

```text
ciallo_ohos/control
```

推荐 Payload 使用 `mode`（阈值/开关）或 `control`（单次动作）其中一种格式：

```json
{
    "mode": {
        "enabled": true,
        "soil_on": 1200,
        "soil_off": 1600,
        "light_on": 300,
        "light_off": 500,
        "temp_on": 30.0,
        "temp_off": 27.0,
        "ch2o_on": 0.20,
        "ch2o_off": 0.15,
        "co2_on": 1200,
        "co2_off": 800,
        "co2_night_on": 1200,
        "co2_night_off": 800,
        "ph_min": 5.5,
        "ph_max": 8.5,
        "ec_min": 0,
        "ec_max": 5000,
        "n_min": 0,
        "n_max": 3000,
        "p_min": 0,
        "p_max": 3000,
        "k_min": 0,
        "k_max": 3000,
        "fan_speed": 80
    }
}
```

说明：
- `mode` 用于配置自动控制阈值与 `enabled` 开关。
- `control` 用于触发一次性执行器动作，不依赖 `enabled`。
- 仅支持两种格式：`mode` 或 `control`（互斥，不支持同一条消息同时包含二者）。

#### 直接控制执行器的简洁格式

除了上面的完整格式，还支持只下发 `control` 的简洁命令，例如：

```json
{"control": {"pump": 1}}
{"control": {"led": 0}}
{"control": {"fan": 60}}
{"control": {"buzzer": 1}}
{"control": {"sg90_angle": 90}}
{"control": {"capture": 1}}
```

说明：
- 直接控制命令不依赖 `enabled` 开关，即使自动控制关闭也会立即执行一次；
- 若自动控制处于开启状态，下一次周期内仍会按阈值逻辑更新执行器状态（即手动只是一时刻的“插手”）。

注意：设备侧只有在 MQTT 已连接时才会订阅并处理命令（本工程由 ETS 调用 `connectMqtt()` 建立连接）。

## 传感器数据获取

所有传感器数据统一通过 ETS/NAPI 接口 `getDataByKey(key: string)` 获取，无需单独初始化。数据由 ESP32-S3 采集，经 WiFi UDP 广播到主板。

### ETS/NAPI 接口（@ohos.myproject）

```typescript
function getDataByKey(key: string): number;
```

获取指定键名对应的传感器数据。失败或数据无效时返回 -1。

### 支持的参数表

| 参数键 | 数据含义 | 数据范围 | 数据类型 | 备注 |
|--------|---------|---------|---------|------|
| `Humi` | 相对湿度（%RH） | 0-100 | float | DHT 传感器 |
| `Temp` | 环境温度（℃） | -40~80 | float | DHT 传感器 |
| `CH2O` | 甲醛浓度（μg/m³） | 0-10000 | float | JW01 模块 |
| `TVOC` | 总挥发性有机物（ppb） | 0-65000 | float | JW01 模块 |
| `CO_2` | 二氧化碳浓度（ppm） | 400-5000 | float | JW01 模块 |
| `SoilHumi` | 土壤湿度（%） | 0-100 | float | 土壤多参数传感器 |
| `SoilTemp` | 土壤温度（℃） | -40~80 | float | 土壤多参数传感器 |
| `EC` | 电导率（μS/cm） | 0-20000 | int | 土壤多参数传感器 |
| `pH` | 酸碱度 | 0-14 | float | 土壤多参数传感器 |
| `N` | 氮含量（mg/kg） | 0-9999 | int | 土壤多参数传感器 |
| `P` | 磷含量（mg/kg） | 0-9999 | int | 土壤多参数传感器 |
| `K` | 钾含量（mg/kg） | 0-9999 | int | 土壤多参数传感器 |
| `Salt` | 盐度（mg/kg） | 0-20000 | int | 土壤多参数传感器 |
| `TDS` | 总溶解固体（mg/L） | 0-20000 | int | 土壤多参数传感器 |
| `Light` | 光照强度（%） | 0-100 | float | 光敏转换 |

### 使用示例

```typescript
// 获取土壤湿度
const soilHumi = myproject.getDataByKey("SoilHumi");
if (soilHumi >= 0) {
    console.log(`土壤湿度: ${soilHumi}%`);
}

// 获取环境温度
const temp = myproject.getDataByKey("Temp");
if (temp >= 0) {
    console.log(`环境温度: ${temp}℃`);
}

// 获取光照强度
const light = myproject.getDataByKey("Light");
if (light >= 0) {
    console.log(`光照强度: ${light}%`);
}
```

说明：
- ESP32-S3 每秒采集一次所有传感器数据，通过 UDP 广播发送（格式见 `esp32_s3/src/main.cpp`）。
- 主板通过 WiFi UDP 接收数据并缓存，ETS 侧通过 `getDataByKey()` 读取缓存值。
- 若某个数据长时间未更新（与之前的采集周期超出阈值），该键返回 -1 表示数据无效。

## SG90舵机控制

**头文件**: `drivers/inc/sg90.h`

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

**头文件**: `drivers/inc/pump_control.h`

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

**头文件**: `app/inc/myserial.h`

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
通过UART发送数据。

```c
unsigned char* return_recv(int* len);
```
返回接收到的数据。`len`为接收数据长度指针，返回值为接收数据缓冲区。
**注意**: 调用者负责释放返回的内存。

## LED控制

**头文件**: `drivers/inc/led_control.h`

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

**头文件**: `drivers/inc/fan_control.h`

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

**头文件**: `drivers/inc/buzzer_control.h`

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

## LLaMA客户端

**头文件**: `app/inc/llama_client.h`

### 结构体

```cpp
struct LlamaRequestParams
```
LLaMA请求参数结构体：
- `prompt`: 字符串，用户输入的提示文本
- `stream`: 布尔值，是否使用流式返回，默认为false

```cpp
struct LlamaResponse
```
LLaMA响应结构体：
- `text`: 字符串，生成的文本内容
- `finished`: 布尔值，是否已完成生成，默认为false
- `error`: 字符串，错误信息（如果有）
- `role`: 字符串，回复角色(assistant)
- `id`: 字符串，响应ID

### 类型定义

```cpp
using ResponseCallback = std::function<void(const LlamaResponse&)>
```
回调函数类型，用于接收LLaMA服务器的响应。

### 类

```cpp
class LlamaClient
```
用于与局域网内LLaMA服务交互的客户端类。

#### 构造与析构

```cpp
LlamaClient(const std::string& host = "192.168.31.5", 
           int port = 8080,
           const std::string& systemMessage = "",
           float temperature = 0.8f,
           int max_tokens = 1024);
```
构造函数，创建LLaMA客户端实例：
- `host`: LLaMA服务器主机名或IP地址，默认为"192.168.31.5"
- `port`: LLaMA服务器端口号，默认为8080
- `systemMessage`: 可选的system角色消息
- `temperature`: 采样温度，控制文本生成的随机性，默认为0.8
- `max_tokens`: 最大生成令牌数，默认为1024

```cpp
~LlamaClient();
```
析构函数，释放资源并断开连接。

#### 连接管理

```cpp
bool connect();
```
连接到LLaMA服务器。成功返回true，失败返回false。

```cpp
void disconnect();
```
断开与服务器的连接。

```cpp
bool isConnected() const;
```
检查是否已连接到服务器。已连接返回true，未连接返回false。

#### 请求与响应

```cpp
bool sendRequest(const LlamaRequestParams& params, ResponseCallback callback);
```
发送请求到LLaMA服务器：
- `params`: 请求参数
- `callback`: 接收响应的回调函数
成功发送请求返回true，失败返回false。

```cpp
LlamaResponse sendRequestSync(const LlamaRequestParams& params, int timeout_ms = 10000);
```
以同步方式发送请求并等待结果：
- `params`: 请求参数
- `timeout_ms`: 超时时间(毫秒)，默认10秒
返回服务器响应。

#### 错误处理

```cpp
std::string getLastError() const;
```
获取最近的错误信息。

#### 会话管理

```cpp
void clearHistory();
```
清除所有历史消息。

```cpp
const std::vector<std::pair<std::string, std::string>>& getHistory() const;
```
获取当前消息历史，返回消息历史列表。