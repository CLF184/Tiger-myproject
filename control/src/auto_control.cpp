#include "auto_control.h"

#include <atomic>
#include <chrono>
#include <cstring>
#include <mutex>
#include <string>
#include <thread>
#include <utility>
#include <ctime>

#include "buzzer_control.h"
#include "fan_control.h"
#include "led_control.h"
#include "light_sensor.h"
#include "mqtt_global.h"
#include "mqtt_payload_builder.h"
#include "myserial.h"
#include "pump_control.h"
#include "sg90.h"

namespace control {

namespace {
std::atomic<bool> g_running{false};
std::atomic<bool> g_enabled{false};

std::mutex g_mutex;
AutoControlThresholds g_thresholds;
std::string g_cmdTopic;

// 记录上一次输出，配合迟滞阈值避免抖动
bool g_pumpOn = false;
bool g_ledOn = false;
bool g_fanOn = false;
bool g_buzzerOn = false;
bool g_shadeOn = false;  // 遮阳板当前是否处于遮挡状态

// 简单的昼/夜判断：本地时间 6:00-18:00 视为白天，其余为夜间
bool IsDaytime()
{
    std::time_t now = std::time(nullptr);
    struct tm t;
    std::memset(&t, 0, sizeof(t));
    // 使用本地时间判断昼夜，比 UTC 更贴近实际日照
    if (!localtime_r(&now, &t)) {
        return true; // 出错时保守地按白天处理
    }
    int hour = t.tm_hour; // 0-23
    return (hour >= 6 && hour < 18);
}

bool ParseBoolField(const std::string &json, const char *key, bool *out)
{
    if (!key || !out) return false;
    const std::string k = std::string("\"") + key + "\"";
    auto pos = json.find(k);
    if (pos == std::string::npos) return false;
    pos = json.find(':', pos);
    if (pos == std::string::npos) return false;
    pos++;
    while (pos < json.size() && (json[pos] == ' ' || json[pos] == '\t')) pos++;

    if (json.compare(pos, 4, "true") == 0) {
        *out = true;
        return true;
    }
    if (json.compare(pos, 5, "false") == 0) {
        *out = false;
        return true;
    }
    if (pos < json.size() && (json[pos] == '0' || json[pos] == '1')) {
        *out = (json[pos] == '1');
        return true;
    }
    return false;
}

bool ParseNumberField(const std::string &json, const char *key, double *out)
{
    if (!key || !out) return false;
    const std::string k = std::string("\"") + key + "\"";
    auto pos = json.find(k);
    if (pos == std::string::npos) return false;
    pos = json.find(':', pos);
    if (pos == std::string::npos) return false;
    pos++;
    while (pos < json.size() && (json[pos] == ' ' || json[pos] == '\t')) pos++;

    const char *start = json.c_str() + pos;
    char *endp = nullptr;
    double v = std::strtod(start, &endp);
    if (endp == start) return false;
    *out = v;
    return true;
}

// 提取形如 "key": { ... } 的子对象内容，用于支持 {"mode": { ... }} 这种包装格式。
bool ExtractObjectForKey(const std::string &json, const char *key, std::string *out)
{
    if (!key || !out) return false;
    const std::string k = std::string("\"") + key + "\"";
    auto pos = json.find(k);
    if (pos == std::string::npos) return false;
    pos = json.find(':', pos);
    if (pos == std::string::npos) return false;
    pos++;
    // 跳过空白
    while (pos < json.size() && (json[pos] == ' ' || json[pos] == '\t' || json[pos] == '\n' || json[pos] == '\r')) pos++;
    if (pos >= json.size() || json[pos] != '{') return false;

    size_t start = pos;
    int depth = 0;
    for (size_t i = pos; i < json.size(); ++i) {
        char c = json[i];
        if (c == '{') {
            depth++;
        } else if (c == '}') {
            depth--;
            if (depth == 0) {
                *out = json.substr(start, i - start + 1);
                return true;
            }
        }
    }
    return false;
}

void ClampThresholds(AutoControlThresholds &t)
{
    if (t.soil_on < 0) t.soil_on = 0;
    if (t.soil_off < 0) t.soil_off = 0;
    if (t.light_on < 0) t.light_on = 0;
    if (t.light_off < 0) t.light_off = 0;

    if (t.fan_speed < 0) t.fan_speed = 0;
    if (t.fan_speed > 100) t.fan_speed = 100;

    // 确保迟滞方向合理（on <= off）
    if (t.soil_on > t.soil_off) std::swap(t.soil_on, t.soil_off);
    if (t.light_on > t.light_off) std::swap(t.light_on, t.light_off);
    if (t.temp_on < t.temp_off) std::swap(t.temp_on, t.temp_off);
    if (t.ch2o_on < t.ch2o_off) std::swap(t.ch2o_on, t.ch2o_off);
    if (t.co2_on < t.co2_off) std::swap(t.co2_on, t.co2_off);
    if (t.co2_night_on < t.co2_night_off) std::swap(t.co2_night_on, t.co2_night_off);

    // 营养/酸碱阈值：确保 min <= max，若全为 0 则认为未配置，不强行调整
    if (t.ph_min > t.ph_max && !(t.ph_min == 0.0 && t.ph_max == 0.0)) std::swap(t.ph_min, t.ph_max);
    if (t.ec_min > t.ec_max && !(t.ec_min == 0.0 && t.ec_max == 0.0)) std::swap(t.ec_min, t.ec_max);
    if (t.n_min > t.n_max && !(t.n_min == 0.0 && t.n_max == 0.0)) std::swap(t.n_min, t.n_max);
    if (t.p_min > t.p_max && !(t.p_min == 0.0 && t.p_max == 0.0)) std::swap(t.p_min, t.p_max);
    if (t.k_min > t.k_max && !(t.k_min == 0.0 && t.k_max == 0.0)) std::swap(t.k_min, t.k_max);
}

void ApplyCommandJson(const std::string &jsonRaw)
{
    // 兼容三种格式：
    // 1) 扁平阈值：{"enabled":true,"soil_on":1200,...}
    // 2) 阈值包装：{"mode": {"enabled":true,"soil_on":1200,...}}
    // 3) 单次控制包装：{"control": {"led":1,"pump":0,...}}

    // 先解析阈值和开关（mode）
    std::string modeObj;
    const std::string *modeJsonPtr = &jsonRaw;
    if (ExtractObjectForKey(jsonRaw, "mode", &modeObj)) {
        modeJsonPtr = &modeObj;
    }
    const std::string &modeJson = *modeJsonPtr;

    bool enabled;
    if (ParseBoolField(modeJson, "enabled", &enabled)) {
        g_enabled.store(enabled);
    }

    AutoControlThresholds next;
    {
        std::lock_guard<std::mutex> lock(g_mutex);
        next = g_thresholds;
    }

    double v;
    if (ParseNumberField(modeJson, "soil_on", &v)) next.soil_on = static_cast<int>(v);
    if (ParseNumberField(modeJson, "soil_off", &v)) next.soil_off = static_cast<int>(v);
    if (ParseNumberField(modeJson, "light_on", &v)) next.light_on = static_cast<int>(v);
    if (ParseNumberField(modeJson, "light_off", &v)) next.light_off = static_cast<int>(v);
    if (ParseNumberField(modeJson, "temp_on", &v)) next.temp_on = v;
    if (ParseNumberField(modeJson, "temp_off", &v)) next.temp_off = v;
    if (ParseNumberField(modeJson, "ch2o_on", &v)) next.ch2o_on = v;
    if (ParseNumberField(modeJson, "ch2o_off", &v)) next.ch2o_off = v;
    // CO2：兼容旧字段 co2_on/co2_off，同时可单独配置夜间 co2_night_on/co2_night_off
    if (ParseNumberField(modeJson, "co2_on", &v)) next.co2_on = v;
    if (ParseNumberField(modeJson, "co2_off", &v)) next.co2_off = v;
    if (ParseNumberField(modeJson, "co2_night_on", &v)) next.co2_night_on = v;
    if (ParseNumberField(modeJson, "co2_night_off", &v)) next.co2_night_off = v;
    // 可选：养分/酸碱报警阈值
    if (ParseNumberField(modeJson, "ph_min", &v)) next.ph_min = v;
    if (ParseNumberField(modeJson, "ph_max", &v)) next.ph_max = v;
    if (ParseNumberField(modeJson, "ec_min", &v)) next.ec_min = v;
    if (ParseNumberField(modeJson, "ec_max", &v)) next.ec_max = v;
    if (ParseNumberField(modeJson, "n_min", &v)) next.n_min = v;
    if (ParseNumberField(modeJson, "n_max", &v)) next.n_max = v;
    if (ParseNumberField(modeJson, "p_min", &v)) next.p_min = v;
    if (ParseNumberField(modeJson, "p_max", &v)) next.p_max = v;
    if (ParseNumberField(modeJson, "k_min", &v)) next.k_min = v;
    if (ParseNumberField(modeJson, "k_max", &v)) next.k_max = v;
    if (ParseNumberField(modeJson, "fan_speed", &v)) next.fan_speed = static_cast<int>(v);

    ClampThresholds(next);
    {
        std::lock_guard<std::mutex> lock(g_mutex);
        g_thresholds = next;
    }

    // 再解析单次控制（control）。若没有 control，则兼容老格式：直接在同一个 JSON 里找字段。
    std::string controlObj;
    const std::string *ctrlJsonPtr = &jsonRaw;
    if (ExtractObjectForKey(jsonRaw, "control", &controlObj)) {
        ctrlJsonPtr = &controlObj;
    } else if (&modeJson != &jsonRaw) {
        // 若存在 mode 包装但没有 control，单次控制字段不会重复出现在 mode 中，这里直接返回即可。
        ctrlJsonPtr = &modeJson;
    }
    const std::string &json = *ctrlJsonPtr;

    // 额外支持一种简洁的“直接控制执行器”格式：根据 pump/led/fan/buzzer/capture 字段立刻动作一次
    // 例如：{"control":{"pump":1}} / {"control":{"led":0}} / {"control":{"fan":60}} / {"control":{"buzzer":1}}
    // 也兼容旧格式：{"pump":1} / {"led":0} / {"fan":60} / {"buzzer":1} / {"capture":1}

    // pump: 0/1 -> 关/开
    if (ParseNumberField(json, "pump", &v)) {
        bool on = (v != 0.0);
        g_pumpOn = on;
        if (on) {
            (void)pump_on();
        } else {
            (void)pump_off();
        }
    }

    // led: 0/1 -> 关/开
    if (ParseNumberField(json, "led", &v)) {
        bool on = (v != 0.0);
        g_ledOn = on;
        if (on) {
            (void)LedOn();
        } else {
            (void)LedOff();
        }
    }

    // fan: 0-100 -> 0 代表停；>0 代表以对应速度正转
    if (ParseNumberField(json, "fan", &v)) {
        int sp = static_cast<int>(v);
        if (sp < 0) sp = 0;
        if (sp > 100) sp = 100;

        {
            std::lock_guard<std::mutex> lock(g_mutex);
            g_thresholds.fan_speed = sp;
        }

        if (sp > 0) {
            g_fanOn = true;
            (void)controlMotor(MOTOR_FORWARD, sp);
        } else {
            g_fanOn = false;
            (void)setMotorDirection(MOTOR_STOP);
        }
    }

    // buzzer: 0/1 -> 关/开
    if (ParseNumberField(json, "buzzer", &v)) {
        bool on = (v != 0.0);
        g_buzzerOn = on;
        (void)BuzzerControl(on ? 1 : 0);
    }

    // sg90_angle: 0-180 -> 设置舵机角度
    if (ParseNumberField(json, "sg90_angle", &v)) {
        int angle = static_cast<int>(v);
        if (angle < SG90_MIN_ANGLE) angle = SG90_MIN_ANGLE;
        if (angle > SG90_MAX_ANGLE) angle = SG90_MAX_ANGLE;
        (void)SG90_SetAngle(angle);
    }

    // capture: 非 0 触发一次拍照指令
    if (ParseNumberField(json, "capture", &v)) {
        if (v != 0.0) {
            const char *cmd = "CAPTURE";
            write_uart(cmd, std::strlen(cmd));
        }
    }
}

void OnMqttMessage(void * /*ctx*/, const char *topic, const void *data, size_t size)
{
    if (!topic || !data || size == 0) return;

    std::string currentTopic;
    {
        std::lock_guard<std::mutex> lock(g_mutex);
        currentTopic = g_cmdTopic;
    }
    if (!currentTopic.empty() && currentTopic != topic) {
        return;
    }

    std::string json(static_cast<const char *>(data), size);
    ApplyCommandJson(json);
}

std::thread g_thread;

void ControlLoop()
{
    mqttc::MqttCClient &mqtt = mqttc::GetMqttClient();
    mqtt.setMessageCallback(OnMqttMessage, nullptr);

    bool subscribed = false;
    bool lastEnabled = false;

    while (g_running.load()) {
        // 尝试准备命令主题
        std::string cmdTopic;
        {
            std::lock_guard<std::mutex> lock(g_mutex);
            cmdTopic = g_cmdTopic;
        }
        if (cmdTopic.empty()) {
            // 默认使用不带 device_id 的公共控制主题
            cmdTopic = std::string("ciallo_ohos/control");
            {
                std::lock_guard<std::mutex> lock(g_mutex);
                if (g_cmdTopic.empty()) g_cmdTopic = cmdTopic;
            }
        }

        // MQTT pump + 订阅（不额外起线程）
        if (mqtt.isConnected()) {
            if (!subscribed) {
                std::string err;
                subscribed = mqtt.subscribe(cmdTopic, 0, &err);
            }
            (void)mqtt.syncOnce(nullptr);
        } else {
            subscribed = false;
        }

        const bool enabled = g_enabled.load();
        if (enabled) {
            AutoControlThresholds t;
            {
                std::lock_guard<std::mutex> lock(g_mutex);
                t = g_thresholds;
            }

            const bool isDay = IsDaytime();

            // 传感器读数
            const double soil = static_cast<double>(get_data_by_key(const_cast<char *>("SoilHumi")));
            const double temp = static_cast<double>(get_data_by_key(const_cast<char *>("Temp")));
            const double co2 = static_cast<double>(get_data_by_key(const_cast<char *>("CO_2")));

            int lightValue = 0;
            bool haveLight = (light_sensor_read(&lightValue) == 0);

            // 启用瞬间：用当前读数初始化状态，避免迟滞区间沿用旧状态
            if (!lastEnabled) {
                g_pumpOn = (soil <= t.soil_on);
                g_ledOn = haveLight ? (lightValue <= t.light_on) : false;
                const double co2OnInit = isDay ? t.co2_on : t.co2_night_on;
                // 白天：温度或 CO2 超标任一条件满足则开启
                // 夜间：更偏向温度控制，只有温度或 CO2 明显超标才开
                g_fanOn = (temp >= t.temp_on) || (co2 >= co2OnInit);
                // 遮阳只在白天根据光照+温度判断，夜间强制收起
                if (isDay) {
                    g_shadeOn = haveLight && (lightValue >= t.light_off) && (temp >= t.temp_on);
                } else {
                    g_shadeOn = false;
                }
            }

            // 水泵：soil 低于 soil_on 开，高于 soil_off 关
            if (soil <= t.soil_on) g_pumpOn = true;
            if (soil >= t.soil_off) g_pumpOn = false;
            if (g_pumpOn) {
                (void)pump_on();
            } else {
                (void)pump_off();
            }

            // LED：light 低于 light_on 开，高于 light_off 关
            if (haveLight) {
                if (lightValue <= t.light_on) g_ledOn = true;
                if (lightValue >= t.light_off) g_ledOn = false;
            }
            if (g_ledOn) {
                (void)LedOn();
            } else {
                (void)LedOff();
            }

            // 风扇：
            // - 白天：使用 co2_on/co2_off
            // - 夜间：使用 co2_night_on/co2_night_off
            if (isDay) {
                if (temp >= t.temp_on || co2 >= t.co2_on) g_fanOn = true;
                if (temp <= t.temp_off && co2 <= t.co2_off) g_fanOn = false;
            } else {
                if (temp >= t.temp_on || co2 >= t.co2_night_on) g_fanOn = true;
                if (temp <= t.temp_off && co2 <= t.co2_night_off) g_fanOn = false;
            }
            if (g_fanOn) {
                (void)controlMotor(MOTOR_FORWARD, t.fan_speed);
            } else {
                (void)setMotorDirection(MOTOR_STOP);
            }
            // 甲醛阈值不再参与自动蜂鸣器控制，蜂鸣器仅通过手动/命令触发

            // 舵机遮阳：仅在白天根据光照+温度控制，夜间强制收起遮阳板
            if (haveLight) {
                bool needShade = false;
                if (isDay) {
                    if (lightValue >= t.light_off && temp >= t.temp_on) {
                        needShade = true;
                    } else if (lightValue <= t.light_on && temp <= t.temp_off) {
                        needShade = false;
                    } else {
                        needShade = g_shadeOn;
                    }
                } else {
                    needShade = false;
                }

                if (needShade != g_shadeOn) {
                    g_shadeOn = needShade;
                    int angle = g_shadeOn ? 135 : 0; // 简单两档：0° 收起，135° 遮阳
                    (void)SG90_SetAngle(angle);
                }
            }
        }

        lastEnabled = enabled;

        std::this_thread::sleep_for(std::chrono::milliseconds(AUTO_CONTROL_PERIOD_MS));
    }
}

} // namespace

void SetAutoControlEnabled(bool enabled)
{
    g_enabled.store(enabled);
}

bool GetAutoControlEnabled()
{
    return g_enabled.load();
}

void SetThresholds(const AutoControlThresholds &t)
{
    AutoControlThresholds next = t;
    ClampThresholds(next);
    std::lock_guard<std::mutex> lock(g_mutex);
    g_thresholds = next;
}

AutoControlThresholds GetThresholds()
{
    std::lock_guard<std::mutex> lock(g_mutex);
    return g_thresholds;
}

void SetCommandTopic(const char *topic)
{
    std::lock_guard<std::mutex> lock(g_mutex);
    g_cmdTopic = topic ? std::string(topic) : std::string();
}

void Start()
{
    bool expected = false;
    if (!g_running.compare_exchange_strong(expected, true)) {
        return;
    }
    g_thread = std::thread(ControlLoop);
}

void Stop()
{
    bool expected = true;
    if (!g_running.compare_exchange_strong(expected, false)) {
        return;
    }
    if (g_thread.joinable()) {
        g_thread.join();
    }
}

} // namespace control
