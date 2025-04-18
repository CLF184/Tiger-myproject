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

    /**
     * 读取土壤湿度传感器原始值
     * @returns 土壤湿度值或null（如果读取失败）
     */
    function ReadSoilMoistureRaw(): number | null;

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

    /**
     * 读取光敏传感器数值
     * @returns 光照强度值或null（如果读取失败）
     */
    function readLightSensor(): number | null;

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
     * @returns 与键名对应的数值
     */
    function getDataByKey(key: string): number;

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
}

export default myproject;
