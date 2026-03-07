import myproject from '@ohos.myproject';

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
  private connectionStatus: MqttConnectionStatus = MqttConnectionStatus.DISCONNECTED;
  private brokerUrl: string = '';
  private clientId: string = '';
  private username: string = '';
  private password: string = '';
  private topicPrefix: string = 'ciallo_ohos'; // 默认 topic 前缀（命名空间）

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
  
  // 设置 topic 前缀（兼容旧方法名：setTopic/getTopic）
  public setTopic(topic: string): void {
    if (!topic) {
      return;
    }

    const trimmed = topic.trim();
    if (trimmed === '') {
      return;
    }

    // 只取第一段作为 prefix（例如输入 ciallo_ohos 或 ciallo_ohos/xxx 都视为 ciallo_ohos）
    const slash = trimmed.indexOf('/');
    this.topicPrefix = slash > 0 ? trimmed.substring(0, slash) : trimmed;
    console.info(`MQTT: topic 前缀已设置为 ${this.topicPrefix}`);
  }

  public getTopic(): string {
    return this.topicPrefix;
  }

  private getTopicPrefixOrDefault(): string {
    const prefix = this.topicPrefix && this.topicPrefix.trim() !== '' ? this.topicPrefix.trim() : 'ciallo_ohos';
    return prefix;
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

      // 配置原生 MQTT 客户端（MQTT-C）
      const cfgStatus = myproject.configMqtt(this.brokerUrl, this.clientId, this.username, this.password);
      if (cfgStatus !== 0) {
        console.error(`MQTT: 原生配置失败, status=${cfgStatus}`);
        this.connectionStatus = MqttConnectionStatus.CONNECTION_FAILED;
        return false;
      }

      const ok = await myproject.connectMqtt();
      if (ok) {
        console.info('MQTT连接成功(原生)');
        this.connectionStatus = MqttConnectionStatus.CONNECTED;
        return true;
      }

      console.error('MQTT连接失败(原生)');
      this.connectionStatus = MqttConnectionStatus.CONNECTION_FAILED;
      return false;
    } catch (error) {
      console.error(`MQTT连接异常: ${error}`);
      this.connectionStatus = MqttConnectionStatus.CONNECTION_FAILED;
      return false;
    }
  }

  // 断开MQTT连接
  public async disconnect(): Promise<boolean> {
    try {
      myproject.disconnectMqtt();
      this.connectionStatus = MqttConnectionStatus.DISCONNECTED;
      console.info('MQTT断开连接成功(原生)');
      return true;
    } catch (error) {
      console.error(`MQTT断开连接异常: ${error}`);
      return false;
    }
  }

  // 重新连接
  public async reconnect(): Promise<boolean> {
    this.connectionStatus = MqttConnectionStatus.CONNECTING;
    return this.connect();
  }

  // 发布传感器消息（组包/采集由原生侧完成）
  public async publishSensor(qos: number = 0): Promise<boolean> {
    if (this.connectionStatus !== MqttConnectionStatus.CONNECTED) {
      console.error('MQTT: 未连接到服务器，无法发布消息');
      return false;
    }

    try {
      const prefix = this.getTopicPrefixOrDefault();
      // publishMqtt(topic, haveImage, qos)
      // haveImage=false：原生侧采集并按 Qt 端所需 JSON 格式发布
      const ok = await myproject.publishMqtt(prefix, false, qos);
      if (ok) {
        console.info(`MQTT消息发布成功: prefix=${prefix}`);
        return true;
      } else {
        console.error('MQTT消息发布失败(原生)');
        this.connectionStatus = MqttConnectionStatus.CONNECTION_LOST;
        return false;
      }
    } catch (error) {
      console.error(`MQTT消息发布异常: ${error}`);
      this.connectionStatus = MqttConnectionStatus.CONNECTION_LOST;
      return false;
    }
  }

  // 上传传感器数据（组包/采集由原生侧完成）
  public async uploadSensorData(): Promise<boolean> {
    return this.publishSensor(1);
  }

  // 上传图片数据 - 原生侧固定读取 PHOTO_PATH 并做 Base64 编码
  public async uploadImage(): Promise<boolean> {
    if (this.connectionStatus !== MqttConnectionStatus.CONNECTED) {
      console.error('MQTT: 未连接到服务器，无法发布图片');
      return false;
    }

    try {
      const prefix = this.getTopicPrefixOrDefault();
      console.info(`MQTT: 开始发布图片(作为传感器JSON的可选字段, PHOTO_PATH), prefix=${prefix}`);

      // publishMqtt(topic, haveImage, qos)
      // 当 haveImage=true 时，原生侧会读取 PHOTO_PATH，Base64 后写入 JSON 的 image 字段再发布。
      const ok = await myproject.publishMqtt(prefix, true, 1);
      if (ok) {
        console.info(`MQTT图片发布成功(含传感器JSON): prefix=${prefix}`);
        return true;
      } else {
        console.error('MQTT图片发布失败(原生)');
        this.connectionStatus = MqttConnectionStatus.CONNECTION_LOST;
        return false;
      }
    } catch (error) {
      console.error(`MQTT图片发布异常: ${error instanceof Error ? error.message : String(error)}`);
      this.connectionStatus = MqttConnectionStatus.CONNECTION_LOST;
      return false;
    }
  }
  
  // 销毁MQTT客户端
  public async destroy(): Promise<void> {
    try {
      myproject.disconnectMqtt();
    } catch (error) {
      console.error(`MQTT客户端销毁异常: ${error}`);
    } finally {
      this.connectionStatus = MqttConnectionStatus.DISCONNECTED;
    }
  }
}

// 导出单例
export const mqttService = MqttService.getInstance();
