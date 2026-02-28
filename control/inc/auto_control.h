#ifndef AUTO_CONTROL_H
#define AUTO_CONTROL_H

#include <cstdint>

// 自动控制周期（毫秒），可在编译时通过 -DAUTO_CONTROL_PERIOD_MS=xxx 覆盖
#ifndef AUTO_CONTROL_PERIOD_MS
#define AUTO_CONTROL_PERIOD_MS 1000
#endif

namespace control {

struct AutoControlThresholds {
    // 说明：*_on / *_off 为迟滞阈值，避免频繁抖动。
    // soilMoisture: 数值越小越干（具体以串口上报为准）
    int soil_on = 1200;
    int soil_off = 1600;

    // lightLevel: 数值越小越暗
    int light_on = 300;
    int light_off = 500;

    // temperature: 摄氏度（来自串口键 "Temp"）
    double temp_on = 30.0;
    double temp_off = 27.0;

    // formaldehyde (CH2O): 浓度（来自串口键 "CH2O"）
    double ch2o_on = 0.20;
    double ch2o_off = 0.15;

    // CO2: 浓度（ppm，来自串口键 "CO_2"）
    // co2_on/co2_off：白天的上下限；co2_night_*：夜间的上下限
    // 为了兼容旧字段，若只配置 co2_on/co2_off，则昼夜共用同一组阈值。
    double co2_on = 1200.0;       // 白天高阈值
    double co2_off = 800.0;       // 白天低阈值
    double co2_night_on = 1200.0; // 夜间高阈值（默认与白天相同，可单独调整）
    double co2_night_off = 800.0; // 夜间低阈值

    // 土壤多参数报警阈值：EC、pH、N、P、K 上下限
    // alarm 仅用于 MQTT 上报和前端显示，不直接驱动执行器
    double ph_min = 5.5;
    double ph_max = 8.5;
    double ec_min = 0.0;
    double ec_max = 5000.0;
    double n_min = 0.0;
    double n_max = 3000.0;
    double p_min = 0.0;
    double p_max = 3000.0;
    double k_min = 0.0;
    double k_max = 3000.0;

    // 风扇开启时速度（0-100）
    int fan_speed = 80;
};

// 全局开关：false 时阈值不生效（线程仍运行，用于接收配置/保持状态）。
void SetAutoControlEnabled(bool enabled);
bool GetAutoControlEnabled();

void SetThresholds(const AutoControlThresholds &t);
AutoControlThresholds GetThresholds();

// 启动/停止自动控制线程（幂等）
void Start();
void Stop();

// MQTT 命令主题：默认 ciallo_ohos/control（不带 deviceId）
void SetCommandTopic(const char *topic);

} // namespace control

#endif
