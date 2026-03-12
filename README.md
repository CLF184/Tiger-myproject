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
- `app/`：业务能力层，包含：
    - **数据通信（同一层）**：`sensor_data_provider`（统一数据通道抽象，`UDP` 与 `SERIAL` 互斥二选一）、`wifi_udp_receiver`（UDP 广播收发）、`myserial`（串口收发）
  - **MQTT 通信**：`mqttc_client`（MQTT-C 客户端包装）、`mqtt_global`（全局实例管理）、`mqtt_payload_builder`（消息负载构建）
  - **AI 能力**：`llama_client`（LLaMA 服务客户端）
- `esp32_s3/`：传感器采集与 UDP 广播固件代码（PlatformIO 工程）。
- `ets/pages/` + `qt/`：前端页面与上位机侧联调入口。

## 目录
- [自动控制（设备侧闭环）](#自动控制设备侧闭环)
- [MQTT 通信模块](#mqtt-通信模块)
- [传感器数据获取](#传感器数据获取)
- [SG90舵机控制](#sg90舵机控制)
- [水泵控制](#水泵控制)
- [串口通信](#串口通信)
- [UDP通信（wifi_udp_receiver）](#udp通信wifi_udp_receiver)
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

## MQTT 通信模块

### MQTT-C 客户端包装（mqttc_client）

**头文件**: `app/inc/mqttc_client.h`  
**实现**: `app/src/mqttc_client.cpp`

对 MQTT-C 库的 C++ 包装，提供完整的 MQTT 连接、发送、接收等能力：

#### 核心类：MqttCClient

```cpp
class MqttCClient {
public:
    // 配置 MQTT 连接参数
    void configure(const std::string &brokerUrl,
                   const std::string &clientId,
                   const std::string &username,
                   const std::string &password);

    // 连接到 MQTT Broker
    bool connect(std::string *errorMsg = nullptr);
    
    // 断开连接
    void disconnect();

    // 发布消息
    bool publish(const std::string &topic,
                 const void *data,
                 size_t size,
                 int qos,
                 bool retain,
                 std::string *errorMsg = nullptr);

    // 订阅主题
    bool subscribe(const std::string &topic, int qos = 0, std::string *errorMsg = nullptr);

    // 发布设备发现公告消息（自动生成 JSON 格式，topic 为 <prefix>/announce/<deviceId>）
    // 使用 retain=true，方便客户端订阅后立即获取最近一次公告
    bool publishDiscoveryAnnounceRetained(const std::string &topicPrefix,
                                          const std::string &deviceId,
                                          const std::string &deviceType,
                                          std::string *errorMsg = nullptr);

    // 单次同步处理：刷新 socket 读写与回调分发（建议在外部循环中周期性调用）
    bool syncOnce(std::string *errorMsg = nullptr);

    // 设置消息回调函数
    void setMessageCallback(MessageCallback cb, void *ctx);

    // 连接状态查询
    bool isConnected() const;
    
    // 获取最后的错误信息
    std::string getLastError() const;
};
```

#### 使用示例

```cpp
#include "mqttc_client.h"

mqttc::MqttCClient client;

// 1. 配置
client.configure("mqtt://192.168.1.100:1883", "device_001", "user", "pass");

// 2. 连接
std::string errMsg;
if (!client.connect(&errMsg)) {
    printf("Connect failed: %s\n", errMsg.c_str());
    return;
}

// 3. 发布消息
const char *payload = "Hello MQTT";
client.publish("test/topic", payload, strlen(payload), 1, false);

// 4. 定期调用 syncOnce 以处理消息
while (client.isConnected()) {
    client.syncOnce();
    usleep(100000); // 100ms
}

// 5. 断开
client.disconnect();
```

### 全局 MQTT 客户端管理（mqtt_global）

**头文件**: `app/inc/mqtt_global.h`  
**实现**: `app/src/mqtt_global.cpp`

为避免同一进程中多个 MQTT 客户端实例，提供全局单例管理：

```cpp
namespace mqttc {

// 获取全局 MQTT-C 客户端实例（单例）
MqttCClient &GetMqttClient();

}
```

#### 使用示例

```cpp
#include "mqtt_global.h"

// 任何地方都可以获取同一个全局实例
mqttc::MqttCClient &client = mqttc::GetMqttClient();
client.publish("device/sensor", data, size, 1, false);
```

### MQTT 消息负载构建（mqtt_payload_builder）

**头文件**: `app/inc/mqtt_payload_builder.h`  
**实现**: `app/src/mqtt_payload_builder.cpp`

提供快速构建符合 Qt 客户端解析格式的 JSON 负载，以及图像 Base64 编码功能：

#### 核心函数

```cpp
namespace mqttc {

// 设置/获取设备 ID（用于 JSON payload）
void SetMqttPayloadDeviceId(const std::string &deviceId);
std::string GetMqttPayloadDeviceId();

// 设置/获取 MQTT 主题前缀，例如 "ciallo_ohos"
// 前缀用于设备特定的主题和发现公告主题
void SetMqttTopicPrefix(const std::string &prefix);
std::string GetMqttTopicPrefix();

// 构建传感器数据 JSON 负载
// 格式：{"deviceId":"...","timestamp":"...","sensors":{...}}
// includeImage: 是否包含最新拍照的 Base64 数据
bool BuildSensorPayloadJson(bool includeImage, std::string &outJson, std::string *errMsg = nullptr);

// 为图像消息构建 Base64 负载
// 自动读取 PHOTO_PATH 指定的文件并进行 Base64 编码
bool BuildImagePayloadBase64(std::string &outBase64, std::string *errMsg = nullptr);

}
```

#### 使用示例

```cpp
#include "mqtt_payload_builder.h"

// 配置
mqttc::SetMqttPayloadDeviceId("device_001");
mqttc::SetMqttTopicPrefix("ciallo_ohos");

// 构建传感器 JSON
std::string sensorJson;
if (mqttc::BuildSensorPayloadJson(false, sensorJson)) {
    // sensorJson 现在包含格式化的 JSON 负载
    client.publish("ciallo_ohos/device_001/sensors", sensorJson.c_str(), sensorJson.size(), 1, false);
}
```

## 传感器数据获取

所有传感器数据统一通过 ETS/NAPI 接口 `getDataByKey(key: string)` 获取，无需单独初始化。数据由 ESP32-S3 采集后进入**同一数据输入层**，可通过 WiFi UDP 或串口两种后端接入。
说明：`UDP` 与 `SERIAL` 为**互斥通道**，任一时刻仅启用一种（由 `sensor_data_provider::SetDataChannel()` 选择）。

### 数据源抽象层（sensor_data_provider）

**头文件**: `app/inc/sensor_data_provider.h`

为适应不同的数据传输方式，系统提供了 `sensor_data_provider` 抽象层，用于屏蔽不同数据源的差异：
- **UDP 模式**（默认）：通过 WiFi UDP 广播接收 ESP32-S3 的传感器数据
- **SERIAL 模式**：通过串口接收传感器数据
- 二者同属一个数据通道层，**互斥二选一**（不是并行叠加）

#### 核心函数

```cpp
// 切换数据源通道
void SetDataChannel(DataChannel channel);

// 获取当前数据源通道
DataChannel GetDataChannel();

// 从当前选定的后端读取传感器数据（由 NAPI 层调用）
float GetDataByKey(const char *key);

// 通过当前选定的后端发送相机捕获命令
int SendCommand(const char *command);
```

补充说明：
- 在调用 `SetDataChannel(...)` 后，`sensor_data_provider` 会启动一次后台查询线程（仅启动一次）。
- 后台线程会周期性通过 `SendCommand("GET_DATA")` 向 ESP32 请求最新数据。
- `GetDataByKey(...)` 仅负责读取当前通道下已缓存/已接收的数据并解析。

#### 数据通道枚举

```cpp
enum class DataChannel {
    SERIAL = 0,  // 串口接收
    UDP = 1,     // UDP 广播接收（默认）
};
```

#### WiFi UDP 接收模块（wifi_udp_receiver）

**头文件**: `app/inc/wifi_udp_receiver.h`

当数据通道为 UDP 模式时，使用此模块监听来自 ESP32-S3 的广播数据：
- **接收端口**：9000
- **发送端口**：9001
- **协议**：UDP 广播
- **数据格式**：纯文本，键值对形式（例如 `Humi:45.3;Temp:25.5;...`）

#### ESP32-S3 端数据发送

ESP32-S3 在 `esp32_s3/src/main.cpp` 中按命令采集并回传传感器数据：
- **触发方式**：收到主控 `GET_DATA` 命令后执行一次采集与回传
- **数据来源**：
  - DHT11（环境温度、湿度）
  - JW01 模块（CH2O、TVOC、CO₂）
  - RS485 土壤多参数传感器（土壤湿度、温度、EC、pH、NPK、盐度、TDS）
  - 光敏传感器（光照强度）
- **传输格式**：
  ```
  Humi:45.3;Temp:25.5;CH2O:10.5;TVOC:45;CO_2:800;SoilHumi:65.0;SoilTemp:22.5;EC:1200;pH:6.5;N:100;P:50;K:80;Salt:200;TDS:500;Light:75.0;
  ```

```c
// 初始化 UDP 接收线程（监听 0.0.0.0:9000）
void init_wifi_udp_receiver(void);

// 获取最近接收到的 UDP 文本数据
int wifi_get_latest_data(char *outBuf, size_t bufLen);

// 通过 UDP 广播发送数据
int wifi_send_broadcast(const char *buf, int len);
```

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
- 主控通过 `sensor_data_provider` 后台线程周期发送 `GET_DATA`，ESP32 收到后返回一帧最新数据。
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

说明：本模块与 `wifi_udp_receiver` 处于同一数据通信层，通过 `sensor_data_provider` 做互斥通道选择；当通道为 `SERIAL` 时才使用本模块作为数据来源。

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

## UDP通信（wifi_udp_receiver）

说明：本模块与 `myserial` 处于同一数据通信层，通过 `sensor_data_provider` 做互斥通道选择；当通道为 `UDP` 时才使用本模块作为数据来源。

**头文件**: `app/inc/wifi_udp_receiver.h`

### 函数

```c
void init_wifi_udp_receiver(void);
```
初始化 UDP 接收线程（监听 `0.0.0.0:9000`）。

```c
int wifi_get_latest_data(char *outBuf, size_t bufLen);
```
获取最近一次收到的 UDP 文本数据。返回 `0` 表示成功，有数据被拷贝；其他值表示当前还没有有效数据。

```c
int wifi_send_broadcast(const char *buf, int len);
```
通过 UDP 广播发送数据。返回 `0` 表示成功，`-1` 表示失败。

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