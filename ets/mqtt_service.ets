import prompt from '@ohos.prompt';
import fs from '@ohos.file.fs'; // 文件系统模块
import util from '@ohos.util';  // 导入工具库，包含 Base64 编码功能
import { 
  MqttClient, 
  MqttClientOptions, 
  MqttConnectOptions, 
  MqttPublishOptions,
  MqttQos,
  MqttAsync
} from '@ohos/mqtt';

// 从mqtt_component引入SensorData接口
import { SensorData } from './mqtt_component';

// MQTT连接状态枚举
export enum MqttConnectionStatus {
  DISCONNECTED,
  CONNECTING,
  CONNECTED,
  CONNECTION_FAILED,
  CONNECTION_LOST
}

// MQTT服务类
export class MqttService {
  private static instance: MqttService;
  private mqttClient: MqttClient | null = null;
  private connectionStatus: MqttConnectionStatus = MqttConnectionStatus.DISCONNECTED;
  private brokerUrl: string = '';
  private clientId: string = '';
  private username: string = '';
  private password: string = '';
  private topic: string = 'ciallo_ohos/sensors'; // 默认主题
  private imageTopic: string = 'ciallo_ohos/image'; // 图片发布主题
  private connectOptions: MqttConnectOptions | null = null;

  // 单例模式
  public static getInstance(): MqttService {
    if (!MqttService.instance) {
      MqttService.instance = new MqttService();
    }
    return MqttService.instance;
  }

  // 获取连接状态
  public getConnectionStatus(): MqttConnectionStatus {
    return this.connectionStatus;
  }

  // 获取连接状态文本描述
  public getConnectionStatusText(): string {
    switch (this.connectionStatus) {
      case MqttConnectionStatus.DISCONNECTED:
        return '未连接';
      case MqttConnectionStatus.CONNECTING:
        return '连接中...';
      case MqttConnectionStatus.CONNECTED:
        return '已连接';
      case MqttConnectionStatus.CONNECTION_FAILED:
        return '连接失败';
      case MqttConnectionStatus.CONNECTION_LOST:
        return '连接断开';
      default:
        return '未知状态';
    }
  }

  // 配置MQTT客户端
  public configureMqtt(brokerUrl: string, clientId: string, username: string, password: string): void {
    this.brokerUrl = brokerUrl;
    this.clientId = clientId;
    this.username = username;
    this.password = password;
  }
  
  // 设置发布主题
  public setTopic(topic: string): void {
    if (topic && topic.trim() !== '') {
      this.topic = topic;
      console.info(`MQTT: 主题已设置为 ${topic}`);
    }
  }
  
  // 获取当前主题
  public getTopic(): string {
    return this.topic;
  }

  // 设置图片发布主题
  public setImageTopic(topic: string): void {
    if (topic && topic.trim() !== '') {
      this.imageTopic = topic;
      console.info(`MQTT: 图片主题已设置为 ${topic}`);
    }
  }

  // 获取图片主题
  public getImageTopic(): string {
    return this.imageTopic;
  }

  // 连接到MQTT服务器
  public async connect(): Promise<boolean> {
    if (this.connectionStatus === MqttConnectionStatus.CONNECTING || 
        this.connectionStatus === MqttConnectionStatus.CONNECTED) {
      console.info('MQTT: 已经在连接或已连接状态');
      return true;
    }

    if (!this.brokerUrl || !this.clientId) {
      console.error('MQTT: 未设置服务器地址或客户端ID');
      return false;
    }

    try {
      this.connectionStatus = MqttConnectionStatus.CONNECTING;

      // 创建MQTT客户端
      const clientOptions: MqttClientOptions = {
        url: this.brokerUrl,
        clientId: this.clientId,
        persistenceType: 1 // 内存持久化
      };

      // 使用MqttAsync创建MQTT客户端
      this.mqttClient = MqttAsync.createMqtt(clientOptions);

      // 配置连接选项
      this.connectOptions = {
        userName: this.username,
        password: this.password,
        cleanSession: true,
        connectTimeout: 30,
        keepAliveInterval: 60,
        automaticReconnect: true,
        minRetryInterval: 1,
        maxRetryInterval: 60
      };

      // 注册连接断开回调
      if (this.mqttClient) {
        this.mqttClient.connectLost((err, data) => {
          console.error(`MQTT连接断开: ${JSON.stringify(data)}`);
          this.connectionStatus = MqttConnectionStatus.CONNECTION_LOST;
          
          // 尝试重连
          setTimeout(() => {
            if (this.connectionStatus !== MqttConnectionStatus.CONNECTED) {
              this.reconnect();
            }
          }, 5000);
        });

        // 连接到MQTT服务器
        const response = await this.mqttClient.connect(this.connectOptions);
        
        if (response && response.code === 0) {
          console.info('MQTT连接成功');
          this.connectionStatus = MqttConnectionStatus.CONNECTED;
          return true;
        } else {
          console.error(`MQTT连接失败: ${JSON.stringify(response)}`);
          this.connectionStatus = MqttConnectionStatus.CONNECTION_FAILED;
          return false;
        }
      } else {
        console.error('MQTT客户端创建失败');
        this.connectionStatus = MqttConnectionStatus.CONNECTION_FAILED;
        return false;
      }
    } catch (error) {
      console.error(`MQTT连接异常: ${error}`);
      this.connectionStatus = MqttConnectionStatus.CONNECTION_FAILED;
      return false;
    }
  }

  // 断开MQTT连接
  public async disconnect(): Promise<boolean> {
    if (!this.mqttClient || this.connectionStatus === MqttConnectionStatus.DISCONNECTED) {
      this.connectionStatus = MqttConnectionStatus.DISCONNECTED;
      return true;
    }

    try {
      const response = await this.mqttClient.disconnect();
      if (response && response.code === 0) {
        console.info('MQTT断开连接成功');
        this.connectionStatus = MqttConnectionStatus.DISCONNECTED;
        return true;
      } else {
        console.error(`MQTT断开连接失败: ${JSON.stringify(response)}`);
        return false;
      }
    } catch (error) {
      console.error(`MQTT断开连接异常: ${error}`);
      return false;
    }
  }

  // 重新连接
  public async reconnect(): Promise<boolean> {
    if (!this.mqttClient) {
      return this.connect();
    }

    try {
      this.connectionStatus = MqttConnectionStatus.CONNECTING;
      const result = await this.mqttClient.reconnect();
      
      if (result) {
        console.info('MQTT重新连接成功');
        this.connectionStatus = MqttConnectionStatus.CONNECTED;
        return true;
      } else {
        console.error('MQTT重新连接失败');
        this.connectionStatus = MqttConnectionStatus.CONNECTION_FAILED;
        return false;
      }
    } catch (error) {
      console.error(`MQTT重新连接异常: ${error}`);
      this.connectionStatus = MqttConnectionStatus.CONNECTION_FAILED;
      return false;
    }
  }

  // 发布消息
  public async publishMessage(topic: string, message: string, qos: number = 0): Promise<boolean> {
    if (!this.mqttClient || this.connectionStatus !== MqttConnectionStatus.CONNECTED) {
      console.error('MQTT: 未连接到服务器，无法发布消息');
      return false;
    }

    try {
      // 将number类型的qos转换为MqttQos枚举
      const mqttQos = qos as MqttQos;
      
      const publishOptions: MqttPublishOptions = {
        topic: topic,
        payload: message,
        qos: mqttQos,
        retained: false
      };

      const response = await this.mqttClient.publish(publishOptions);
      
      if (response && response.code === 0) {
        console.info(`MQTT消息发布成功: Topic=${topic}, Message=${message}`);
        return true;
      } else {
        console.error(`MQTT消息发布失败: ${JSON.stringify(response)}`);
        return false;
      }
    } catch (error) {
      console.error(`MQTT消息发布异常: ${error}`);
      return false;
    }
  }

  // 上传传感器数据
  public async uploadSensorData(sensorData: SensorData): Promise<boolean> {
    try {
      const jsonData = JSON.stringify(sensorData);
      // 使用设置的主题而不是硬编码的主题
      return await this.publishMessage(this.topic, jsonData, 1);
    } catch (error) {
      console.error(`上传传感器数据异常: ${error}`);
      return false;
    }
  }

  // 上传图片数据 - 修改为接收图片路径，并使用Base64编码
  public async uploadImage(imagePath: string): Promise<boolean> {
    if (!this.mqttClient || this.connectionStatus !== MqttConnectionStatus.CONNECTED) {
      console.error('MQTT: 未连接到服务器，无法发布图片');
      return false;
    }

    try {
      console.info(`MQTT: 开始读取图片文件: ${imagePath}`);
      
      // 使用正确的文件读取方法
      // 创建文件描述符
      const file = await fs.open(imagePath, fs.OpenMode.READ_ONLY);
      // 获取文件状态，用于确定文件大小
      const stat = await fs.stat(imagePath);
      // 创建缓冲区
      const buffer = new ArrayBuffer(stat.size);
      // 读取文件到缓冲区
      await fs.read(file.fd, buffer);
      // 关闭文件
      await fs.close(file.fd);
      
      // 使用 util.Base64Helper 进行 Base64 编码
      const uint8Array = new Uint8Array(buffer);
      // 创建 Base64Helper 实例并使用 encodeToStringSync 方法
      const base64Helper = new util.Base64Helper();
      const base64String = base64Helper.encodeToStringSync(uint8Array);
      
      console.info(`MQTT: 图片文件读取成功，大小: ${buffer.byteLength} 字节，Base64编码后大小: ${base64String.length} 字符`);
      
      // 使用支持的 payload 属性发布图片数据
      const publishOptions: MqttPublishOptions = {
        topic: this.imageTopic,
        payload: base64String, // 使用 Base64 编码的字符串
        qos: 1 as MqttQos,
        retained: false
      };

      const response = await this.mqttClient.publish(publishOptions);
      
      if (response && response.code === 0) {
        console.info(`MQTT图片发布成功: Topic=${this.imageTopic}, Size=${buffer.byteLength}字节`);
        return true;
      } else {
        console.error(`MQTT图片发布失败: ${JSON.stringify(response)}`);
        return false;
      }
    } catch (error) {
      console.error(`MQTT图片发布异常: ${error instanceof Error ? error.message : String(error)}`);
      return false;
    }
  }
  
  // 销毁MQTT客户端
  public async destroy(): Promise<void> {
    if (this.mqttClient) {
      try {
        await this.mqttClient.destroy();
        this.mqttClient = null;
        this.connectionStatus = MqttConnectionStatus.DISCONNECTED;
      } catch (error) {
        console.error(`MQTT客户端销毁异常: ${error}`);
      }
    }
  }
}

// 导出单例
export const mqttService = MqttService.getInstance();
