import prompt from '@ohos.prompt';
import { mqttService, MqttConnectionStatus } from './mqtt_service';
import image from '@ohos.multimedia.image';

// 定义传感器接口
export interface Sensors {
  soilMoisture: number;
  lightLevel: number;
  temperature: number;
  humidity: number;
  formaldehyde: number;
  tvoc: number;
  co2: number;
}

// 定义提示消息选项接口
export interface ToastOptions {
  message: string;
  duration: number;
}

@Component
export struct MqttSettings {
  @State brokerUrl: string = 'tcp://broker.emqx.io:1883';
  @State clientId: string = `device_${Math.floor(Math.random() * 1000000)}`;
  @State username: string = '';
  @State password: string = '';
  // 仅设置 topic 前缀（命名空间）：例如把 ciallo_ohos 改为 myproj
  @State topicPrefix: string = 'ciallo_ohos';
  @State isConnected: boolean = false;
  @State connectionStatus: string = '未连接';
  @State autoUpload: boolean = false;
  @State uploadInterval: number = 60; // 上传间隔，默认60秒
  @State lastUploadTime: string = ''; // 最后一次上传时间
  @State isUploading: boolean = false; // 正在上传状态
  @State isImageUploading: boolean = false; // 图片上传状态
  @State lastImageUploadTime: string = ''; // 最后一次图片上传时间

  // 数据源属性，用于接收外部传入的传感器数据
  @Prop soilMoisture: number;
  @Prop lightLevel: number;
  @Prop temperature: number;
  @Prop humidity: number;
  @Prop formaldehyde: number;
  @Prop tvoc: number;
  @Prop co2: number;
  @Prop pixelMap: image.PixelMap | null = null;  // 添加接收PixelMap的属性
  @Prop imageUri: string = '';  // 仅用于 UI 预览/提示（发布由原生固定读取 PHOTO_PATH）

  // 用于定时上传的定时器ID
  private uploadTimer: number = -1;

  aboutToAppear() {
    // 初始化时从MQTT服务获取连接状态
    this.updateConnectionStatus();
    
    // 获取之前配置的主题
    const savedPrefix = mqttService.getTopic();
    if (savedPrefix) {
      this.topicPrefix = savedPrefix;
    }
  }

  aboutToDisappear() {
    // 清除定时器
    if (this.uploadTimer !== -1) {
      clearInterval(this.uploadTimer);
      this.uploadTimer = -1;
    }
  }

  // 更新连接状态显示
  updateConnectionStatus() {
    const status = mqttService.getConnectionStatus();
    this.isConnected = status === MqttConnectionStatus.CONNECTED;
    this.connectionStatus = mqttService.getConnectionStatusText();
  }

  // 连接到MQTT服务器
  async connectToMqtt() {
    // 更新MQTT配置
    mqttService.configureMqtt(this.brokerUrl, this.clientId, this.username, this.password);
    
    // 设置 topic 前缀（命名空间）
    mqttService.setTopic(this.topicPrefix);

    const connected = await mqttService.connect().catch((error: Error | string) => {
      console.error(`连接MQTT异常: ${error}`);
      return false;
    });
    this.updateConnectionStatus();

    prompt.showToast({
      message: connected ? '已连接到MQTT服务器' : '连接MQTT服务器失败',
      duration: 2000
    });
  }

  // 断开MQTT连接
  async disconnectFromMqtt() {
    const ok = await mqttService.disconnect().catch((error: Error | string) => {
      console.error(`断开MQTT异常: ${error}`);
      return false;
    });
    this.updateConnectionStatus();

    prompt.showToast({
      message: ok ? '已断开MQTT连接' : '断开MQTT连接失败',
      duration: 2000
    });

    // 断开连接时停止自动上传
    if (this.uploadTimer !== -1) {
      clearInterval(this.uploadTimer);
      this.uploadTimer = -1;
      this.autoUpload = false;
    }
  }

  // 切换自动上传状态
  toggleAutoUpload(enabled: boolean) {
    this.autoUpload = enabled;
    
    if (enabled) {
      // 先进行一次立即上传
      this.uploadSensorData();
      
      // 设置定时上传
      if (this.uploadTimer !== -1) {
        clearInterval(this.uploadTimer);
      }
      
      // 转换为毫秒
      const intervalMs = this.uploadInterval * 1000;
      this.uploadTimer = setInterval(() => {
        this.uploadSensorData();
      }, intervalMs);
      
      const toastOptions: ToastOptions = {
        message: `已开启自动上传，间隔${this.uploadInterval}秒`,
        duration: 2000
      };
      prompt.showToast(toastOptions);
    } else {
      // 停止定时上传
      if (this.uploadTimer !== -1) {
        clearInterval(this.uploadTimer);
        this.uploadTimer = -1;
      }
      
      const toastOptions: ToastOptions = {
        message: '已停止自动上传',
        duration: 2000
      };
      prompt.showToast(toastOptions);
    }
  }

  // 手动上传传感器数据
  async uploadSensorData() {
    if (!this.isConnected) {
      const toastOptions: ToastOptions = {
        message: '未连接到MQTT服务器，无法上传数据',
        duration: 2000
      };
      prompt.showToast(toastOptions);
      return;
    }
    
    if (this.isUploading) {
      return; // 避免重复上传
    }
    
    this.isUploading = true;

    // 传感器采集与 JSON 组包由原生侧完成（格式与 Qt 客户端一致）
    const success = await mqttService.uploadSensorData().catch((error: Error | string) => {
      console.error(`上传数据异常: ${error}`);
      return false;
    });

    if (success) {
      this.lastUploadTime = new Date().toLocaleTimeString();
    }
    prompt.showToast({
      message: success ? '数据上传成功' : '数据上传失败',
      duration: success ? 1000 : 2000
    });

    this.isUploading = false;
  }

  // 上传当前图片
  async uploadImage() {
    if (!this.isConnected) {
      const toastOptions: ToastOptions = {
        message: '未连接到MQTT服务器，无法上传图片',
        duration: 2000
      };
      prompt.showToast(toastOptions);
      return;
    }
    
    if (this.isImageUploading) {
      return; // 避免重复上传
    }
    
    this.isImageUploading = true;

    // 显示上传中提示
    prompt.showToast({
      message: '图片上传中...',
      duration: 1000
    });

    // 原生侧固定读取 PHOTO_PATH 并做 Base64 编码后发布
    const success = await mqttService.uploadImage().catch((error: Error | string) => {
      console.error(`上传图片异常: ${error}`);
      return false;
    });

    if (success) {
      this.lastImageUploadTime = new Date().toLocaleTimeString();
    }
    prompt.showToast({
      message: success ? '图片上传成功' : '图片上传失败',
      duration: success ? 1000 : 2000
    });

    this.isImageUploading = false;
  }

  build() {
    Column() {
      Text('MQTT数据上传').fontSize(20).fontWeight(FontWeight.Medium).margin({ bottom: 10 })
      
      // MQTT服务器配置
      Row() {
        Text('服务器地址:').fontSize(16).width('30%')
        TextInput({ text: this.brokerUrl })
          .width('70%')
          .onChange((value) => {
            this.brokerUrl = value;
          })
          .enabled(!this.isConnected)
      }.width('100%').margin({ bottom: 8 })
      
      Row() {
        Text('客户端ID:').fontSize(16).width('30%')
        TextInput({ text: this.clientId })
          .width('70%')
          .onChange((value) => {
            this.clientId = value;
          })
          .enabled(!this.isConnected)
      }.width('100%').margin({ bottom: 8 })
      
      Row() {
        Text('用户名:').fontSize(16).width('30%')
        TextInput({ text: this.username })
          .width('70%')
          .onChange((value) => {
            this.username = value;
          })
          .enabled(!this.isConnected)
      }.width('100%').margin({ bottom: 8 })
      
      Row() {
        Text('密码:').fontSize(16).width('30%')
        TextInput({ 
          text: this.password,
          placeholder: '请输入密码'
        })
          .width('70%')
          .onChange((value) => {
            this.password = value;
          })
          .type(InputType.Password)
          .enabled(!this.isConnected)
      }.width('100%').margin({ bottom: 12 })
      
      // 添加主题配置行
      Row() {
        Text('Topic前缀:').fontSize(16).width('30%')
        TextInput({
          text: this.topicPrefix,
          placeholder: '例如 ciallo_ohos（仅前缀）'
        })
          .width('70%')
          .onChange((value) => {
            this.topicPrefix = value;
            // 实时更新服务中的前缀设置
            mqttService.setTopic(value);
          })
      }.width('100%').margin({ bottom: 8 })
      
      // 连接状态和操作按钮
      Row() {
        Text(`状态: ${this.connectionStatus}`)
          .fontSize(16)
          .fontColor(this.isConnected ? '#4CAF50' : '#F44336')
        
        if (!this.isConnected) {
          Button('连接')
            .onClick(() => {
              this.connectToMqtt();
            })
            .margin({ left: 8 })
        } else {
          Button('断开')
            .onClick(() => {
              this.disconnectFromMqtt();
            })
            .margin({ left: 8 })
        }
      }.width('100%').justifyContent(FlexAlign.SpaceBetween).margin({ bottom: 16 })
      
      // 自动上传设置
      Row() {
        Text('自动上传:').fontSize(16)
        Toggle({ type: ToggleType.Switch, isOn: this.autoUpload })
          .enabled(this.isConnected)
          .onChange((isOn) => {
            this.toggleAutoUpload(isOn);
          })
      }.width('100%').justifyContent(FlexAlign.SpaceBetween).margin({ bottom: 8 })
      
      if (this.autoUpload) {
        // 上传间隔设置
        Row() {
          Text(`上传间隔(秒): ${this.uploadInterval}`).fontSize(16)
          Slider({
            value: this.uploadInterval,
            min: 5,
            max: 300,
            step: 5
          })
            .width('70%')
            .onChange((value) => {
              this.uploadInterval = value;
              
              // 更新定时器
              if (this.autoUpload && this.uploadTimer !== -1) {
                clearInterval(this.uploadTimer);
                const intervalMs = this.uploadInterval * 1000;
                this.uploadTimer = setInterval(() => {
                  this.uploadSensorData();
                }, intervalMs);
              }
            })
        }.width('100%').margin({ bottom: 8 })
      }
      
      // 手动上传按钮和上传状态
      Row() {
        Button({
          type: ButtonType.Normal,
          stateEffect: true
        }) {
          Row() {
            if (this.isUploading) {
              LoadingProgress().width(16).height(16).margin({ right: 5 })
            }
            Text('立即上传数据').fontSize(14)
          }
        }
        .enabled(this.isConnected && !this.isUploading)
        .onClick(() => {
          this.uploadSensorData();
        })
        
        if (this.lastUploadTime) {
          Text(`上次: ${this.lastUploadTime}`).fontSize(14)
        }
      }.width('100%').justifyContent(FlexAlign.SpaceBetween).margin({ top: 8 })
      
      // 添加图片上传按钮
      Divider().color('#CCCCCC').margin({ top: 12, bottom: 12 })
      
      Row() {
        Button({
          type: ButtonType.Normal,
          stateEffect: true
        }) {
          Row() {
            if (this.isImageUploading) {
              LoadingProgress().width(16).height(16).margin({ right: 5 })
            }
            Text('上传当前图片').fontSize(14)
          }
        }
        .enabled(this.isConnected && !this.isImageUploading)
        .onClick(() => {
          this.uploadImage();
        })
        
        if (this.lastImageUploadTime) {
          Text(`上次: ${this.lastImageUploadTime}`).fontSize(14)
        }
      }.width('100%').justifyContent(FlexAlign.SpaceBetween).margin({ top: 8 })
      
      if (this.imageUri) {
        Text(`预览图片: ${this.imageUri.split('/').pop()}`).fontSize(12).fontColor('#4CAF50').margin({ top: 4 })
      }
      Text('发送图片时：原生读取 PHOTO_PATH 并写入传感器JSON的 image 字段').fontSize(12).fontColor('#666666').margin({ top: 4 })
    }
    .width('90%')
    .padding(16)
    .backgroundColor('#f5f5f5')
    .borderRadius(8)
  }
}
