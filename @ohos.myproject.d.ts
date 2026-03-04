/* Copyright 2022 Unionman Technology Co., Ltd.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 */

declare namespace myproject {
    /**
     * 初始化所有硬件模块
     * @returns 0表示成功，非0表示失败
     */
    function initAllModules(): number;

    // /**
    //  * 读取土壤湿度传感器原始值
    //  * @returns 土壤湿度值或null（如果读取失败）
    //  */
    // function ReadSoilMoistureRaw(): number | null;

    /**
     * 设置SG90舵机角度
     * @param angle 角度值(0-180)
     * @returns 0表示成功，非0表示失败
     */
    function setSG90Angle(angle: number): number;

    /**
     * 打开水泵
     * @returns 0表示成功，非0表示失败
     */
    function pumpOn(): number;

    /**
     * 关闭水泵
     * @returns 0表示成功，非0表示失败
     */
    function pumpOff(): number;

    // /**
    //  * 读取光敏传感器数值
    //  * @returns 光照强度值或null（如果读取失败）
    //  */
    // function readLightSensor(): number | null;

    /**
     * 打开LED灯
     * @returns 0表示成功，非0表示失败
     */
    function ledOn(): number;

    /**
     * 关闭LED灯
     * @returns 0表示成功，非0表示失败
     */
    function ledOff(): number;

    /**
     * 控制风扇方向和速度
     * @param direction 风扇方向: 0-停止, 1-正转, 2-反转
     * @param speed 风扇速度: 0-100(百分比)，默认为80
     * @returns 0表示成功，非0表示失败
     */
    function controlFan(direction: number, speed?: number): number;

    /**
     * 打开蜂鸣器
     * @returns 0表示成功，非0表示失败
     */
    function buzzeron(): number;

    /**
     * 关闭蜂鸣器
     * @returns 0表示成功，非0表示失败
     */
    function buzzeroff(): number;

    /**
     * 发送拍照指令到串口
     * @returns 0表示成功，非0表示失败
     */
    function sendCapture(): number;

    /**
     * 根据关键字获取数据
     * @param key 数据的键名
        * @example 获取土壤湿度：getDataByKey("SoilHumi")
     * @returns 与键名对应的数值
     */
    function getDataByKey(key: string): number;

    /**
     * 注册回调：当底层图片接收完成并写入文件后触发
     * @param callback 回调入参为图片文件路径（例如 filesDir/output.jpeg）
     * @returns 0表示成功，负数表示失败
     */
    function onImageCaptured(callback: (path: string) => void): number;

    /**
     * 取消注册图片回调
     * @returns 0表示成功
     */
    function offImageCaptured(): number;

    /**
     * 向LLaMA服务器发送请求并获取回复
     * @param prompt 提示文本
     * @returns Promise，解析为生成的回复文本或null（如果请求失败）
     */
    function askLlama(prompt: string): Promise<string | null>;

    /**
     * 配置LLaMA服务器连接参数
     * @param host 服务器地址
     * @param port 服务器端口，默认8080
     * @param systemMessage 系统提示消息，默认为"你是一个非常有用的助手"
     * @param temperature 采样温度(0.0-1.0)，默认0.8
     * @param maxTokens 生成回复的最大令牌数，默认1024
     * @returns 0表示成功，-1表示创建实例失败，-2表示连接服务器失败
     */
    function configLlama(host: string, port: number, systemMessage?: string, temperature?: number, maxTokens?: number): number;
    
    /**
     * 清除LLaMA对话历史
     * @returns 0表示成功，-1表示LlamaClient未初始化
     */
    function clearLlamaHistory(): number;

    /**
     * 配置MQTT连接参数
     * @param brokerUrl 例如 tcp://host:1883
     * @param clientId 作为 deviceId 使用
     * @param username 用户名（可选）
     * @param password 密码（可选）
     * @returns 0表示成功，负数表示参数错误
     */
    function configMqtt(brokerUrl: string, clientId: string, username?: string, password?: string): number;

    /**
     * 连接MQTT
     */
    function connectMqtt(): Promise<boolean>;

    /**
     * 断开MQTT
     */
    function disconnectMqtt(): number;

    /**
     * 当前是否已连接
     */
    function isMqttConnected(): boolean;

    /**
     * 发布消息
     * - haveImage=false: 原生侧采集传感器并按 Qt 客户端所需 JSON 格式发布
     * - haveImage=true: 原生侧读取 PHOTO_PATH，Base64 后作为 JSON 的可选字段 image 一并发布
     */
    function publishMqtt(topic: string, haveImage: boolean, qos?: number): Promise<boolean>;

    /**
     * 自动控制：全局开关
     * @param enabled true 启用阈值控制；false 禁用阈值控制
     * @returns 0 表示成功
     */
    function setAutoControlEnabled(enabled: boolean): number;

    /**
     * 获取自动控制全局开关
     */
    function getAutoControlEnabled(): boolean;

    /**
     * 设置自动控制阈值（字段可选，未提供的字段保持不变）
     * on/off 为迟滞阈值，避免抖动。
     */
    function setAutoControlThresholds(cfg: {
        soil_on?: number;
        soil_off?: number;
        light_on?: number;
        light_off?: number;
        temp_on?: number;
        temp_off?: number;
        ch2o_on?: number;
        ch2o_off?: number;
        co2_on?: number;
        co2_off?: number;
        co2_night_on?: number;
        co2_night_off?: number;
        ph_min?: number;
        ph_max?: number;
        ec_min?: number;
        ec_max?: number;
        n_min?: number;
        n_max?: number;
        p_min?: number;
        p_max?: number;
        k_min?: number;
        k_max?: number;
        fan_speed?: number;
    }): number;

    /**
     * 获取当前自动控制阈值
     */
    function getAutoControlThresholds(): {
        soil_on: number;
        soil_off: number;
        light_on: number;
        light_off: number;
        temp_on: number;
        temp_off: number;
        ch2o_on: number;
        ch2o_off: number;
        co2_on: number;
        co2_off: number;
        co2_night_on: number;
        co2_night_off: number;
        ph_min: number;
        ph_max: number;
        ec_min: number;
        ec_max: number;
        n_min: number;
        n_max: number;
        p_min: number;
        p_max: number;
        k_min: number;
        k_max: number;
        fan_speed: number;
    };

    /**
     * 设置 MQTT 命令下发主题（Qt 可向该 topic 发布阈值 JSON）
    * 为空则使用默认：ciallo_ohos/control（不带 deviceId）
     */
    function setAutoControlCommandTopic(topic: string): number;
}

export default myproject;
