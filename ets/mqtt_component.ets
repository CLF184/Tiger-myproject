import prompt from '@ohos.prompt';
import { mqttService, MqttConnectionStatus } from './mqtt_service';
import fs from '@ohos.file.fs';
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

// 定义传感器数据接口
export interface SensorData {
  deviceId: string;
  timestamp: string;
  sensors: Sensors;
}

// 定义提示消息选项接口
export interface ToastOptions {
  message: string;
  duration: number;
}

@Component
export struct MqttSettings {
  @State brokerUrl: string = 'tcp://192.168.31.5:1883';
  @State clientId: string = `device_${Math.floor(Math.random() * 1000000)}`;
  @State username: string = '';
  @State password: string = '';
  @State topic: string = 'ciallo_ohos/sensors';
  @State imageTopic: string = 'ciallo_ohos/image';  // 添加图片主题状态变量，默认为/image
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
  @Prop imageUri: string = '';  // 添加接收图片路径的属性

  // 用于定时上传的定时器ID
  private uploadTimer: number = -1;

  aboutToAppear() {
    // 初始化时从MQTT服务获取连接状态
    this.updateConnectionStatus();
    
    // 获取之前配置的主题
    const savedTopic = mqttService.getTopic();
    if (savedTopic) {
      this.topic = savedTopic;
    }
    
    // 获取之前配置的图片主题
    const savedImageTopic = mqttService.getImageTopic();
    if (savedImageTopic) {
      this.imageTopic = savedImageTopic;
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
    
    // 设置主题
    mqttService.setTopic(this.topic);
    
    // 设置图片主题
    mqttService.setImageTopic(this.imageTopic);
    
    try {
      const connected = await mqttService.connect();
      this.updateConnectionStatus();
      
      if (connected) {
        const toastOptions: ToastOptions = {
          message: '已连接到MQTT服务器',
          duration: 2000
        };
        prompt.showToast(toastOptions);
      } else {
        const toastOptions: ToastOptions = {
          message: '连接MQTT服务器失败',
          duration: 2000
        };
        prompt.showToast(toastOptions);
      }
    } catch (error) {
      console.error(`连接MQTT异常: ${error}`);
      const toastOptions: ToastOptions = {
        message: `连接异常: ${error}`,
        duration: 2000
      };
      prompt.showToast(toastOptions);
    }
  }

  // 断开MQTT连接
  async disconnectFromMqtt() {
    try {
      await mqttService.disconnect();
      this.updateConnectionStatus();
      const toastOptions: ToastOptions = {
        message: '已断开MQTT连接',
        duration: 2000
      };
      prompt.showToast(toastOptions);
      
      // 断开连接时停止自动上传
      if (this.uploadTimer !== -1) {
        clearInterval(this.uploadTimer);
        this.uploadTimer = -1;
        this.autoUpload = false;
      }
    } catch (error) {
      console.error(`断开MQTT异常: ${error}`);
      const toastOptions: ToastOptions = {
        message: `断开连接异常: ${error}`,
        duration: 2000
      };
      prompt.showToast(toastOptions);
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
    
    try {
      // 构建传感器数据对象，符合SensorData接口
      const sensors: Sensors = {
        soilMoisture: this.soilMoisture,
        lightLevel: this.lightLevel,
        temperature: this.temperature,
        humidity: this.humidity,
        formaldehyde: this.formaldehyde,
        tvoc: this.tvoc,
        co2: this.co2
      };
      
      const sensorData: SensorData = {
        deviceId: this.clientId,
        timestamp: new Date().toISOString(),
        sensors: sensors
      };
      
      const success = await mqttService.uploadSensorData(sensorData);
      
      if (success) {
        this.lastUploadTime = new Date().toLocaleTimeString();
        const toastOptions: ToastOptions = {
          message: '数据上传成功',
          duration: 1000
        };
        prompt.showToast(toastOptions);
      } else {
        const toastOptions: ToastOptions = {
          message: '数据上传失败',
          duration: 2000
        };
        prompt.showToast(toastOptions);
      }
    } catch (error) {
      console.error(`上传数据异常: ${error}`);
      const toastOptions: ToastOptions = {
        message: `上传异常: ${error}`,
        duration: 2000
      };
      prompt.showToast(toastOptions);
    } finally {
      this.isUploading = false;
    }
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
    
    // 检查是否有图片路径
    if (!this.imageUri) {
      const toastOptions: ToastOptions = {
        message: '没有可用的图片',
        duration: 2000
      };
      prompt.showToast(toastOptions);
      return;
    }
    
    this.isImageUploading = true;
    
    try {
      // 显示上传中提示
      const loadingToast: ToastOptions = {
        message: '图片上传中...',
        duration: 1000
      };
      prompt.showToast(loadingToast);
      
      // 直接传递图片文件路径给MQTT服务
      const success = await mqttService.uploadImage(this.imageUri);
      
      if (success) {
        this.lastImageUploadTime = new Date().toLocaleTimeString();
        const toastOptions: ToastOptions = {
          message: '图片上传成功',
          duration: 1000
        };
        prompt.showToast(toastOptions);
      } else {
        const toastOptions: ToastOptions = {
          message: '图片上传失败',
          duration: 2000
        };
        prompt.showToast(toastOptions);
      }
    } catch (error) {
      console.error(`上传图片异常: ${error}`);
      const errorMsg = error instanceof Error ? error.message : String(error);
      const toastOptions: ToastOptions = {
        message: `上传图片异常: ${errorMsg}`,
        duration: 2000
      };
      prompt.showToast(toastOptions);
    } finally {
      this.isImageUploading = false;
    }
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
        Text('发布主题:').fontSize(16).width('30%')
        TextInput({ text: this.topic })
          .width('70%')
          .onChange((value) => {
            this.topic = value;
            // 实时更新服务中的主题设置
            if (value) {
              mqttService.setTopic(value);
            }
          })
      }.width('100%').margin({ bottom: 8 })
      
      // 添加图片主题配置行
      Row() {
        Text('图片主题:').fontSize(16).width('30%')
        TextInput({ text: this.imageTopic })
          .width('70%')
          .onChange((value) => {
            this.imageTopic = value;
            // 实时更新服务中的图片主题设置
            if (value) {
              mqttService.setImageTopic(value);
            }
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
        .enabled(this.isConnected && !this.isImageUploading && this.imageUri !== '')
        .onClick(() => {
          this.uploadImage();
        })
        
        if (this.lastImageUploadTime) {
          Text(`上次: ${this.lastImageUploadTime}`).fontSize(14)
        }
      }.width('100%').justifyContent(FlexAlign.SpaceBetween).margin({ top: 8 })
      
      if (this.imageUri) {
        Text(`图片可用: ${this.imageUri.split('/').pop()}`).fontSize(12).fontColor('#4CAF50').margin({ top: 4 })
      } else {
        Text('无可用图片').fontSize(12).fontColor('#F44336').margin({ top: 4 })
      }
    }
    .width('90%')
    .padding(16)
    .backgroundColor('#f5f5f5')
    .borderRadius(8)
  }
}
