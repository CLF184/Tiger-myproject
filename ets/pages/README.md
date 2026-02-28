# myproject_napi

## 安装说明

本工程的 MQTT 由底层 C++（MQTT-C）实现并通过 NAPI 暴露给 ETS 层使用，
上层无需再安装/依赖 `@ohos/mqtt`。

ETS 侧通过 `@ohos.myproject` 调用以下接口完成 MQTT 操作：
- `configMqtt(brokerUrl, clientId, username?, password?)`
- `connectMqtt()` / `disconnectMqtt()`
- `publishMqtt(topic, haveImage, qos?)`

说明：
- `haveImage=false`：原生侧采集传感器并按既定 JSON 格式发布。
- `haveImage=true`：原生侧读取 `PHOTO_PATH`，Base64 后写入 JSON 的 `image` 字段再发布。