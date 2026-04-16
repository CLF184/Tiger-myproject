#include "auto_control.h"

#include <atomic>
#include <chrono>
#include <cstring>
#include <mutex>
#include <string>
#include <thread>
#include <utility>
#include <algorithm>
#include <ctime>
#include <cmath>

#include "buzzer_control.h"
#include "cJSON.h"
#include "fan_control.h"
#include "led_control.h"
#include "light_sensor.h"
#include "mqtt_global.h"
#include "mqtt_payload_builder.h"
#include "pump_control.h"
#include "sensor_data_provider.h"
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

// 使用静态阈值控制光照（已移除在线自适应逻辑）

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

bool GetBoolField(cJSON *obj, const char *key, bool *out)
{
    if (obj == nullptr || key == nullptr || out == nullptr) {
        return false;
    }

    cJSON *item = cJSON_GetObjectItemCaseSensitive(obj, key);
    if (item == nullptr) {
        return false;
    }

    if (cJSON_IsBool(item)) {
        *out = cJSON_IsTrue(item);
        return true;
    }

    if (cJSON_IsNumber(item)) {
        *out = (item->valuedouble != 0.0);
        return true;
    }

    return false;
}

bool GetNumberField(cJSON *obj, const char *key, double *out)
{
    if (obj == nullptr || key == nullptr || out == nullptr) {
        return false;
    }

    cJSON *item = cJSON_GetObjectItemCaseSensitive(obj, key);
    if (item == nullptr || !cJSON_IsNumber(item)) {
        return false;
    }

    *out = item->valuedouble;
    return true;
}

cJSON *GetObjectField(cJSON *obj, const char *key)
{
    if (obj == nullptr || key == nullptr) {
        return nullptr;
    }

    cJSON *item = cJSON_GetObjectItemCaseSensitive(obj, key);
    if (item == nullptr || !cJSON_IsObject(item)) {
        return nullptr;
    }

    return item;
}

void ClampThresholds(AutoControlThresholds &t)
{
    if (t.soil_on < 0) t.soil_on = 0;
    if (t.soil_off < 0) t.soil_off = 0;
    // 土壤体积含水率按百分比处理（0-100）
    if (t.soil_on > 100) t.soil_on = 100;
    if (t.soil_off > 100) t.soil_off = 100;
    if (t.light_on < 0) t.light_on = 0;
    if (t.light_off < 0) t.light_off = 0;
    if (t.light_on > 100) t.light_on = 100;
    if (t.light_off > 100) t.light_off = 100;
    
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

// ----------------- 模糊控制辅助函数 -----------------
// 返回在区间 [on, off] 内线性下降的“需要度”（用于当变量越低越需要激活的情况，
// 例如土壤湿度：低湿度 -> 更需要灌溉）。当 on >= off 时退化为原始的阈值行为。
double FuzzyLowerDegree(double x, double on, double off)
{
    if (on >= off) {
        return (x <= on) ? 1.0 : 0.0;
    }
    if (x <= on) return 1.0;
    if (x >= off) return 0.0;
    return (off - x) / (off - on);
}

// 返回在区间 [on, off] 内线性上升的“需要度”（用于当变量越高越需要激活的情况，
// 例如温度/CO2/光照：越高越需要风扇/遮阳）。当 on >= off 时退化为原始的阈值行为。
double FuzzyUpperDegree(double x, double on, double off)
{
    if (on >= off) {
        return (x >= on) ? 1.0 : 0.0;
    }
    if (x <= on) return 0.0;
    if (x >= off) return 1.0;
    return (x - on) / (off - on);
}

// 模糊开启/关闭判定的上下阈（可以后续调整为可配置）
static constexpr double kFuzzyOnThreshold = 0.6;
static constexpr double kFuzzyOffThreshold = 0.4;

// 将 degree（0..1）映射为风扇转速（0..maxSpeed）
int DegreeToFanSpeed(double degree, int maxSpeed)
{
    if (degree <= 0.0) return 0;
    if (degree >= 1.0) return maxSpeed;
    return static_cast<int>(std::round(degree * maxSpeed));
}


void ApplyCommandJson(const std::string &jsonRaw)
{
    // 仅支持两种格式（互斥）：
    // 1) 仅 mode：{"mode": {"enabled":true,"soil_on":30,...}} （soil_on/off 单位为百分比 0-100）
    // 2) 仅 control：{"control": {"led":1,"pump":0,...}}

    cJSON *root = cJSON_Parse(jsonRaw.c_str());
    if (root == nullptr || !cJSON_IsObject(root)) {
        cJSON_Delete(root);
        return;
    }

    cJSON *modeObj = GetObjectField(root, "mode");
    cJSON *controlObj = GetObjectField(root, "control");
    if (modeObj == nullptr && controlObj == nullptr) {
        cJSON_Delete(root);
        return;
    }
    if (modeObj != nullptr && controlObj != nullptr) {
        cJSON_Delete(root);
        return;
    }

    double v;

    // 先解析阈值和开关（mode）
    if (modeObj != nullptr) {
        bool enabled;
        if (GetBoolField(modeObj, "enabled", &enabled)) {
            g_enabled.store(enabled);
        }

        AutoControlThresholds next;
        {
            std::lock_guard<std::mutex> lock(g_mutex);
            next = g_thresholds;
        }

        if (GetNumberField(modeObj, "soil_on", &v)) next.soil_on = static_cast<int>(v);
        if (GetNumberField(modeObj, "soil_off", &v)) next.soil_off = static_cast<int>(v);
        if (GetNumberField(modeObj, "light_on", &v)) next.light_on = static_cast<int>(v);
        if (GetNumberField(modeObj, "light_off", &v)) next.light_off = static_cast<int>(v);
        if (GetNumberField(modeObj, "temp_on", &v)) next.temp_on = v;
        if (GetNumberField(modeObj, "temp_off", &v)) next.temp_off = v;
        if (GetNumberField(modeObj, "ch2o_on", &v)) next.ch2o_on = v;
        if (GetNumberField(modeObj, "ch2o_off", &v)) next.ch2o_off = v;
        // CO2：白天阈值 co2_on/co2_off，可单独配置夜间 co2_night_on/co2_night_off
        if (GetNumberField(modeObj, "co2_on", &v)) next.co2_on = v;
        if (GetNumberField(modeObj, "co2_off", &v)) next.co2_off = v;
        if (GetNumberField(modeObj, "co2_night_on", &v)) next.co2_night_on = v;
        if (GetNumberField(modeObj, "co2_night_off", &v)) next.co2_night_off = v;
        // 可选：养分/酸碱报警阈值
        if (GetNumberField(modeObj, "ph_min", &v)) next.ph_min = v;
        if (GetNumberField(modeObj, "ph_max", &v)) next.ph_max = v;
        if (GetNumberField(modeObj, "ec_min", &v)) next.ec_min = v;
        if (GetNumberField(modeObj, "ec_max", &v)) next.ec_max = v;
        if (GetNumberField(modeObj, "n_min", &v)) next.n_min = v;
        if (GetNumberField(modeObj, "n_max", &v)) next.n_max = v;
        if (GetNumberField(modeObj, "p_min", &v)) next.p_min = v;
        if (GetNumberField(modeObj, "p_max", &v)) next.p_max = v;
        if (GetNumberField(modeObj, "k_min", &v)) next.k_min = v;
        if (GetNumberField(modeObj, "k_max", &v)) next.k_max = v;
        if (GetNumberField(modeObj, "fan_speed", &v)) next.fan_speed = static_cast<int>(v);

        ClampThresholds(next);
        {
            std::lock_guard<std::mutex> lock(g_mutex);
            g_thresholds = next;
        }
    }

    // 再解析单次控制（control）
    cJSON *json = controlObj;
    if (json == nullptr) {
        cJSON_Delete(root);
        return;
    }

    // 直接控制执行器：根据 pump/led/fan/buzzer/sg90_angle/capture 字段立刻动作一次
    // 例如：{"control":{"pump":1}} / {"control":{"led":0}} / {"control":{"fan":60}} / {"control":{"buzzer":1}}

    // pump: 0/1 -> 关/开
    if (GetNumberField(json, "pump", &v)) {
        bool on = (v != 0.0);
        g_pumpOn = on;
        if (on) {
            (void)pump_on();
        } else {
            (void)pump_off();
        }
    }

    // led: 0/1 -> 关/开
    if (GetNumberField(json, "led", &v)) {
        bool on = (v != 0.0);
        g_ledOn = on;
        if (on) {
            (void)LedOn();
        } else {
            (void)LedOff();
        }
    }

    // fan: 0-100 -> 0 代表停；>0 代表以对应速度正转
    if (GetNumberField(json, "fan", &v)) {
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
    if (GetNumberField(json, "buzzer", &v)) {
        bool on = (v != 0.0);
        g_buzzerOn = on;
        (void)BuzzerControl(on ? 1 : 0);
    }

    // sg90_angle: 0-180 -> 设置舵机角度
    if (GetNumberField(json, "sg90_angle", &v)) {
        int angle = static_cast<int>(v);
        if (angle < SG90_MIN_ANGLE) angle = SG90_MIN_ANGLE;
        if (angle > SG90_MAX_ANGLE) angle = SG90_MAX_ANGLE;
        (void)SG90_SetAngle(angle);
    }

    // capture: 非 0 触发一次拍照指令
    if (GetNumberField(json, "capture", &v)) {
        if (v != 0.0) {
            (void)sensor::SendCommand("CAPTURE");
        }
    }

    cJSON_Delete(root);
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
            // 默认使用带 deviceId 的控制主题（不再回退到公共通道）。
            const std::string deviceId = mqttc::GetMqttPayloadDeviceId();
            std::string prefix = mqttc::GetMqttTopicPrefix();
            if (prefix.empty()) {
                prefix = "ciallo_ohos";
            }
            if (deviceId.empty() || deviceId == "unknown") {
                // 还没有 deviceId 时先不订阅，等待上层初始化/配置填充。
                cmdTopic.clear();
            } else {
                cmdTopic = prefix + "/" + deviceId + "/control";
                std::lock_guard<std::mutex> lock(g_mutex);
                if (g_cmdTopic.empty()) g_cmdTopic = cmdTopic;
            }
        }

        // MQTT pump + 订阅（不额外起线程）
        if (mqtt.isConnected()) {
            if (!subscribed) {
                std::string err;
                if (!cmdTopic.empty()) {
                    subscribed = mqtt.subscribe(cmdTopic, 0, &err);
                }
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
            const double soil = static_cast<double>(sensor::GetDataByKey("SoilHumi"));
            const double temp = static_cast<double>(sensor::GetDataByKey("Temp"));
            const double co2 = static_cast<double>(sensor::GetDataByKey("CO_2"));
            const double light = static_cast<double>(sensor::GetDataByKey("Light"));

            // 使用静态光照阈值（已撤销自适应调整）

            // 启用瞬间：用当前读数初始化状态，避免迟滞区间沿用旧状态
            if (!lastEnabled) {
                g_pumpOn = (soil <= t.soil_on);
                g_ledOn = (light <= t.light_on); // 初始化时使用静态阈值
                const double co2OnInit = isDay ? t.co2_on : t.co2_night_on;
                // 白天：温度或 CO2 超标任一条件满足则开启
                // 夜间：更偏向温度控制，只有温度或 CO2 明显超标才开
                g_fanOn = (temp >= t.temp_on) || (co2 >= co2OnInit);
            // 遮阳只在白天根据光照+温度判断，夜间强制收起
                if (isDay) {
                    g_shadeOn = (light >= t.light_off) && (temp >= t.temp_on);
                } else {
                    g_shadeOn = false;
                }
            }

            // 水泵：soil 低于 soil_on 开，高于 soil_off 关
            // 使用模糊控制：根据 soil 在 [soil_on, soil_off] 之间线性计算“需要灌溉度”。
            {
                double soilDegree = FuzzyLowerDegree(soil, static_cast<double>(t.soil_on), static_cast<double>(t.soil_off));
                if (soilDegree >= kFuzzyOnThreshold) g_pumpOn = true;
                else if (soilDegree <= kFuzzyOffThreshold) g_pumpOn = false;

                if (g_pumpOn) {
                    (void)pump_on();
                } else {
                    (void)pump_off();
                }
            }

            // LED：使用模糊控制，light 越低越需要补光
            {
                double lightLowDegree = FuzzyLowerDegree(light, static_cast<double>(t.light_on), static_cast<double>(t.light_off));
                if (lightLowDegree >= kFuzzyOnThreshold) g_ledOn = true;
                else if (lightLowDegree <= kFuzzyOffThreshold) g_ledOn = false;

                if (g_ledOn) {
                    (void)LedOn();
                } else {
                    (void)LedOff();
                }
            }

            // 风扇：
            // - 白天：使用 co2_on/co2_off
            // - 夜间：使用 co2_night_on/co2_night_off
            // 风扇：模糊融合温度与 CO2，需要度按最大或可加权方式合成，最终映射到速度
            {
                double tempDegree = FuzzyUpperDegree(temp, static_cast<double>(t.temp_off), static_cast<double>(t.temp_on));
                double co2ThresholdOn = isDay ? t.co2_on : t.co2_night_on;
                double co2ThresholdOff = isDay ? t.co2_off : t.co2_night_off;
                double co2Degree = FuzzyUpperDegree(co2, static_cast<double>(co2ThresholdOff), static_cast<double>(co2ThresholdOn));

                // 简单规则：需要度取两者最大值（若任一项高则开启），也可以改为加权平均
                double needDegree = std::max(tempDegree, co2Degree);

                // 根据 needDegree 计算风扇速度并使用模糊开/关迟滞
                int targetSpeed = DegreeToFanSpeed(needDegree, t.fan_speed);
                if (needDegree >= kFuzzyOnThreshold) g_fanOn = true;
                else if (needDegree <= kFuzzyOffThreshold) g_fanOn = false;

                if (g_fanOn && targetSpeed > 0) {
                    (void)controlMotor(MOTOR_FORWARD, targetSpeed);
                } else {
                    (void)setMotorDirection(MOTOR_STOP);
                }
            }
            // 甲醛阈值不再参与自动蜂鸣器控制，蜂鸣器仅通过手动/命令触发

            // 舵机遮阳：仅在白天根据光照+温度控制，夜间强制收起遮阳板
            {
                bool needShade = false;
                if (isDay) {
                    double lightHighDegree = FuzzyUpperDegree(light, static_cast<double>(t.light_on), static_cast<double>(t.light_off));
                    double tempHighDegree = FuzzyUpperDegree(temp, static_cast<double>(t.temp_off), static_cast<double>(t.temp_on));
                    // 需要遮阳当光强与温度同时较高，取 min 表示“且”逻辑
                    double shadeDegree = std::min(lightHighDegree, tempHighDegree);
                    if (shadeDegree >= kFuzzyOnThreshold) needShade = true;
                    else if (shadeDegree <= kFuzzyOffThreshold) needShade = false;
                    else needShade = g_shadeOn;
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
