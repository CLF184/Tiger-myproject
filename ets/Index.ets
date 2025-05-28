import myproject from '@ohos.myproject';
import fs from '@ohos.file.fs';
import common from '@ohos.app.ability.common';
import image from '@ohos.multimedia.image';  // 添加图片处理模块导入
import prompt from '@ohos.prompt';
import { ChatMessage, ChatBubble } from './llama_chat'; // 引入新创建的聊天组件
import { MqttSettings } from './mqtt_component'; // 引入MQTT设置组件

// 获取应用文件路径
let context = getContext(this) as common.UIAbilityContext;
let filesDir = context.filesDir;
// filesDir = /data/storage/el2/base/haps/entry/files


@Entry
@Component
struct Index {
  @State soilMoisture: number = 0;
  @State lightLevel: number = 0;
  @State temperature: number = 0;
  @State humidity: number = 0;
  @State formaldehyde: number = 0;
  @State tvoc: number = 0;
  @State co2: number = 0;
  @State waterPumpActive: boolean = false;
  @State ledActive: boolean = false;
  @State fanStatus: string = 'OFF';
  @State fanSpeed: number = 0;  // 添加风扇速度状态
  @State fanDirection: boolean = true;  // 添加风扇方向状态，true表示正转，false表示反转
  @State fanActive: boolean = false;    // 添加风扇启动状态
  @State servoAngle: number = 90;
  @State initStatus: string = '未初始化';
  @State imageUri: string = '';  // 保留原有变量用于检查文件路径
  @State pixelMap: image.PixelMap | null = null;  // 添加用于存储 PixelMap 的状态变量
  @State imageLoading: boolean = false;  // 图片加载状态
  @State lastImageRefreshTime: number = 0; // 跟踪最后一次强制刷新图片的时间

  // 添加 LLaMA 相关状态变量
  @State llamaHost: string = '192.168.31.5';
  @State llamaPort: string = '8080';
  @State llamaConnected: boolean = false;
  @State llamaTemperature: string = '0.7';
  @State llamaMax_Tokens: string =  '2048';
  @State llamaSystemMessage: string = "你是一个有用的助手";
  @State llamaPrompt: string = '';
  @State llamaResponse: string = '';
  @State llamaLoading: boolean = false;

  // 添加聊天历史相关变量
  @State chatHistory: ChatMessage[] = []
  chatScroller: Scroller = new Scroller()

  private timer: number = -1;
  private scroller: Scroller = new Scroller();
  private imageTimer: number = -1;  // 用于定时检查图片更新
  private pixelMapTimeout: number = -1;  // 添加超时计时器
  private isPixelMapConverting: boolean = false;  // 添加转换状态跟踪
  private forceRefreshInterval: number = 3000; // 强制刷新图片的间隔时间（3秒）

  aboutToAppear() {
    // 延迟初始化，给系统一些时间准备好硬件接口
    setTimeout(() => {
      this.initHardware();
    }, 1000);

    // 启动定时任务，每2秒更新一次传感器数据
    this.timer = setInterval(() => {
      this.updateSensorData();
    }, 1000);

    // 启动定时任务，每2秒检查一次图片是否更新
    this.imageTimer = setInterval(() => {
      this.checkAndLoadImage();
    }, 2000);

    this.clearLlamaHistory();
  }

  // 检查并加载图片（使用 PixelMap）
  checkAndLoadImage() {
    try {
      const imagePath = filesDir + '/output.jpeg';
      const currentTime = Date.now();

      // 判断是否需要强制刷新图片（每3秒强制刷新一次）
      const shouldForceRefresh = (currentTime - this.lastImageRefreshTime) >= this.forceRefreshInterval;

      fs.access(imagePath)
        .then(() => {
          // 图片存在，当路径变更或需要强制刷新时，重新加载图片
          if (this.imageUri !== imagePath || shouldForceRefresh) {
            this.imageUri = imagePath;
            this.loadPixelMap(imagePath);
            this.lastImageRefreshTime = currentTime; // 更新最后一次刷新时间
            console.info(`${shouldForceRefresh ? '强制刷新' : '加载新'}图片: ${this.imageUri}`);
          }
        })
    } catch (error) {
      console.error(`检查图片异常: ${error}`);
    }
  }

  // 加载图片到 PixelMap
  loadPixelMap(imagePath: string) {
    try {
      // 如果已经在转换中，则不重复操作
      if (this.isPixelMapConverting) {
        console.info('已有正在进行的 PixelMap 转换，跳过');
        return;
      }

      this.imageLoading = true;
      this.isPixelMapConverting = true;

      // 设置超时检测
      if (this.pixelMapTimeout !== -1) {
        clearTimeout(this.pixelMapTimeout);
      }

      this.pixelMapTimeout = setTimeout(() => {
        if (this.isPixelMapConverting) {
          console.warn('PixelMap 转换超时 (5秒)，尝试重新转换');
          this.isPixelMapConverting = false;
          this.imageLoading = false;
          // 延迟一点再重试，避免立即重试可能导致的循环问题
          setTimeout(() => {
            this.loadPixelMap(imagePath);
          }, 500);
        }
      }, 1000);

      // 创建图片源对象
      const imageSource = image.createImageSource(imagePath);
      if (imageSource) {
        // 释放之前的 PixelMap 资源
        if (this.pixelMap) {
          this.pixelMap.release();
          this.pixelMap = null;
        }

        // 创建新的 PixelMap
        imageSource.createPixelMap()
          .then(pixelMap => {
            clearTimeout(this.pixelMapTimeout);
            this.pixelMap = pixelMap;
            this.imageLoading = false;
            this.isPixelMapConverting = false;
            console.info('成功加载图片到 PixelMap');
          })
      }
    } catch (error) {
      clearTimeout(this.pixelMapTimeout);
      this.imageLoading = false;
      this.isPixelMapConverting = false;
      console.error(`加载 PixelMap 异常: ${error}`);
    }
  }

  initHardware() {
    try {
      console.info('开始初始化硬件模块...');
      const result = myproject.initAllModules();
      console.info(`initAllModules 返回值: ${result}`);

      if (result === 0) {
        this.initStatus = '初始化成功';
        console.info('硬件模块初始化成功');
      } else {
        this.initStatus = `初始化失败(错误码:${result})`;
        console.error(`硬件模块初始化失败，错误码: ${result}`);

        // 尝试重新初始化
        setTimeout(() => {
          console.info('尝试重新初始化...');
          this.initHardware();
        }, 3000);
      }
    } catch (error) {
      this.initStatus = `初始化异常: ${error}`;
      console.error(`硬件模块初始化异常: ${error}, 错误类型: ${typeof error}`);
    }
  }

  aboutToDisappear() {
    if (this.timer !== -1) {
      clearInterval(this.timer);
      this.timer = -1;
    }

    if (this.imageTimer !== -1) {
      clearInterval(this.imageTimer);
      this.imageTimer = -1;
    }

    if (this.pixelMapTimeout !== -1) {
      clearTimeout(this.pixelMapTimeout);
      this.pixelMapTimeout = -1;
    }

    // 释放 PixelMap 资源
    if (this.pixelMap) {
      this.pixelMap.release();
      this.pixelMap = null;
    }
  }

  updateSensorData() {
    try {
      const soilData = myproject.ReadSoilMoistureRaw();
      if (soilData !== null) {
        this.soilMoisture = soilData;
      }
    } catch (error) {
      console.error(`读取土壤湿度失败: ${error}`);
    }

    try {
      const lightData = myproject.readLightSensor();
      if (lightData !== null) {
        this.lightLevel = lightData;
      }
    } catch (error) {
      console.error(`读取光照传感器失败: ${error}`);
    }

    try {
      const tempData = myproject.getDataByKey('Temp');
      if (tempData !== null) {
        this.temperature = tempData;
      }
    } catch (error) {
      console.error(`读取温度失败: ${error}`);
    }

    try {
      const humiData = myproject.getDataByKey('Humi');
      if (humiData !== null) {
        this.humidity = humiData;
      }
    } catch (error) {
      console.error(`读取湿度失败: ${error}`);
    }

    try {
      const ch2oData = myproject.getDataByKey('CH2O');
      if (ch2oData !== null) {
        this.formaldehyde = ch2oData;
      }
    } catch (error) {
      console.error(`读取甲醛浓度失败: ${error}`);
    }

    try {
      const tvocData = myproject.getDataByKey('TVOC');
      if (tvocData !== null) {
        this.tvoc = tvocData;
      }
    } catch (error) {
      console.error(`读取TVOC失败: ${error}`);
    }

    try {
      const co2Data = myproject.getDataByKey('CO_2');
      if (co2Data !== null) {
        this.co2 = co2Data;
      }
    } catch (error) {
      console.error(`读取CO2浓度失败: ${error}`);
    }
  }

  // 水泵控制
  turnOnWaterPump() {
    try {
      const result = myproject.pumpOn();
      this.waterPumpActive = (result === 0);
      console.info(`打开水泵: ${result === 0 ? '成功' : '失败'}`);
    } catch (error) {
      console.error(`打开水泵异常: ${error}`);
      this.waterPumpActive = false;
    }
  }

  turnOffWaterPump() {
    try {
      const result = myproject.pumpOff();
      this.waterPumpActive = !(result === 0);
      console.info(`关闭水泵: ${result === 0 ? '成功' : '失败'}`);
    } catch (error) {
      console.error(`关闭水泵异常: ${error}`);
    }
  }

  // LED控制
  turnOnLed() {
    try {
      const result = myproject.ledOn();
      this.ledActive = (result === 0);
      console.info(`打开LED: ${result === 0 ? '成功' : '失败'}`);
    } catch (error) {
      console.error(`打开LED异常: ${error}`);
      this.ledActive = false;
    }
  }

  turnOffLed() {
    try {
      const result = myproject.ledOff();
      this.ledActive = !(result === 0);
      console.info(`关闭LED: ${result === 0 ? '成功' : '失败'}`);
    } catch (error) {
      console.error(`关闭LED异常: ${error}`);
    }
  }

  // 更新风扇控制方法
  controlFan() {
    try {
      // 如果风扇未激活，设置方向为0（停止）
      if (!this.fanActive) {
        const result = myproject.controlFan(0, 0);
        this.fanStatus = 'OFF';
        console.info(`风扇停止: ${result === 0 ? '成功' : '失败'}`);
        return;
      }

      // 风扇激活时，根据方向设置
      const direction = this.fanDirection ? 1 : 2; // 1 - 正转, 2 - 反转
      const result = myproject.controlFan(direction, this.fanSpeed);

      this.fanStatus = this.fanDirection ? '正转' : '反转';
      console.info(`风扇控制(方向:${direction}, 速度:${this.fanSpeed}): ${result === 0 ? '成功' : '失败'}`);
    } catch (error) {
      console.error(`控制风扇异常: ${error}`);
      this.fanStatus = '错误';
    }
  }

  // 蜂鸣器控制
  buzzerOn() {
    try {
      const result = myproject.buzzeron();
      console.info(`打开蜂鸣器: ${result === 0 ? '成功' : '失败'}`);
    } catch (error) {
      console.error(`打开蜂鸣器异常: ${error}`);
    }
  }

  buzzerOff() {
    try {
      const result = myproject.buzzeroff();
      console.info(`关闭蜂鸣器: ${result === 0 ? '成功' : '失败'}`);
    } catch (error) {
      console.error(`关闭蜂鸣器异常: ${error}`);
    }
  }

  // 舵机控制
  setServoAngle(angle: number) {
    try {
      // 确保角度在有效范围内
      const validAngle = Math.min(Math.max(angle, 0), 180);
      const result = myproject.setSG90Angle(validAngle);
      console.info(`设置舵机角度 ${validAngle}°: ${result === 0 ? '成功' : '失败'}`);
    } catch (error) {
      console.error(`设置舵机角度异常: ${error}`);
    }
  }

  // 拍照功能
  captureImage() {
    try {
      const result = myproject.sendCapture();
      console.info(`发送拍照指令: ${result === 0 ? '成功' : '失败'}`);
    } catch (error) {
      console.error(`发送拍照指令异常: ${error}`);
    }
  }

  // 连接到 LLaMA 服务器
  connectToLlama() {
    try {
      this.llamaLoading = true;
      const result = myproject.configLlama(this.llamaHost,
        Number.parseInt(this.llamaPort),
        this.llamaSystemMessage,
        Number.parseInt(this.llamaTemperature),
        Number.parseInt(this.llamaMax_Tokens));
      this.llamaConnected = (result === 0);
      console.info(`LLaMA 服务器连接 ${this.llamaConnected ? '成功' : '失败'}`);
    }finally {
      this.llamaLoading = false;
    }
  }

  // 向 LLaMA 发送请求
  askLlama() {
    if (!this.llamaPrompt.trim()) {
      return;
    }

    // 添加用户消息到历史记录
    this.chatHistory.push(new ChatMessage(this.llamaPrompt, true));

    // 滚动到最新消息
    this.scrollToBottom();

    // 准备接收AI回复
    this.llamaLoading = true;

    // 记录当前提问，然后清空输入框
    const currentPrompt = this.llamaPrompt;
    this.llamaPrompt = '';

    try {
      myproject.askLlama(currentPrompt)
        .then((response) => {
          if (response) {
            // 添加AI回复到历史记录
            this.chatHistory.push(new ChatMessage(response, false));
            // 更新当前显示的回复
            this.llamaResponse = response;
          } else {
            // 添加错误消息到历史记录
            this.chatHistory.push(new ChatMessage('没有收到回复', false));
            this.llamaResponse = '没有收到回复';
          }

          // 滚动到最新消息
          this.scrollToBottom();
        })
        .finally(() => {
          this.llamaLoading = false;
        });
    } catch (error) {
      console.error(`发送 LLaMA 请求异常: ${error}`);
      // 添加错误消息到历史记录
      this.chatHistory.push(new ChatMessage(`错误: ${error}`, false));
      this.llamaResponse = `错误: ${error}`;
      this.llamaLoading = false;

      // 滚动到最新消息
      this.scrollToBottom();
    }
  }

  // 滚动到聊天记录底部
  scrollToBottom() {
    this.chatScroller.scrollToIndex(this.chatHistory.length - 1);
  }

  // 修改清除LLaMA历史记录的方法
  clearLlamaHistory() {
    try {
      const result = myproject.clearLlamaHistory();
      console.info(`清除LLaMA对话历史: ${result === 0 ? '成功' : '失败'}`);

      // 清除UI界面上的历史记录
      this.chatHistory = [];
      // 清除当前界面上的响应
      this.llamaResponse = '';

      // 提示用户
      prompt.showToast({
        message: '对话历史已清除',
        duration: 2000,
      });
    } catch (error) {
      console.error(`清除LLaMA对话历史异常: ${error}`);

      // 显示错误提示
      prompt.showToast({
        message: '清除对话历史失败',
        duration: 2000,
      });
    }
  }

  build() {
    Scroll(this.scroller) {
      Column() {
        Text('智能植物照料系统')
          .fontSize(24)
          .fontWeight(FontWeight.Bold)
          .margin({ bottom: 20 })

        // 初始化状态显示
        Text(`系统状态: ${this.initStatus}`)
          .fontSize(16)
          .fontWeight(FontWeight.Medium)
          .margin({ bottom: 16 })

        // 传感器数据显示区域
        Column() {
          Text('传感器数据').fontSize(20).fontWeight(FontWeight.Medium).margin({ bottom: 10 })

          Row() {
            Text('土壤湿度:').fontSize(16)
            Text(`${this.soilMoisture}`).fontSize(16)
          }.width('100%').justifyContent(FlexAlign.SpaceBetween).margin({ bottom: 8 })

          Row() {
            Text('光照强度:').fontSize(16)
            Text(`${this.lightLevel}`).fontSize(16)
          }.width('100%').justifyContent(FlexAlign.SpaceBetween).margin({ bottom: 8 })

          Row() {
            Text('温度:').fontSize(16)
            Text(`${this.temperature}°C`).fontSize(16)
          }.width('100%').justifyContent(FlexAlign.SpaceBetween).margin({ bottom: 8 })

          Row() {
            Text('湿度:').fontSize(16)
            Text(`${this.humidity}%`).fontSize(16)
          }.width('100%').justifyContent(FlexAlign.SpaceBetween).margin({ bottom: 8 })

          Row() {
            Text('甲醛浓度:').fontSize(16)
            Text(`${this.formaldehyde} mg/m³`).fontSize(16)
          }.width('100%').justifyContent(FlexAlign.SpaceBetween).margin({ bottom: 8 })

          Row() {
            Text('TVOC:').fontSize(16)
            Text(`${this.tvoc} ppb`).fontSize(16)
          }.width('100%').justifyContent(FlexAlign.SpaceBetween).margin({ bottom: 8 })

          Row() {
            Text('二氧化碳:').fontSize(16)
            Text(`${this.co2} ppm`).fontSize(16)
          }.width('100%').justifyContent(FlexAlign.SpaceBetween).margin({ bottom: 8 })
        }
        .width('90%')
        .padding(16)
        .backgroundColor('#f5f5f5')
        .borderRadius(8)
        .margin({ bottom: 20 })

        // 水泵控制
        Column() {
          Text('水泵控制').fontSize(20).fontWeight(FontWeight.Medium).margin({ bottom: 10 })

          Row() {
            Toggle({ type: ToggleType.Switch, isOn: this.waterPumpActive })
              .onChange((isOn) => {
                if (isOn) {
                  this.turnOnWaterPump();
                } else {
                  this.turnOffWaterPump();
                }
              })
            Text(this.waterPumpActive ? '已开启' : '已关闭').fontSize(16)
          }
          .width('100%')
          .justifyContent(FlexAlign.SpaceBetween)
        }
        .width('90%')
        .padding(16)
        .backgroundColor('#f5f5f5')
        .borderRadius(8)
        .margin({ bottom: 20 })

        // LED灯控制
        Column() {
          Text('LED灯控制').fontSize(20).fontWeight(FontWeight.Medium).margin({ bottom: 10 })

          Row() {
            Toggle({ type: ToggleType.Switch, isOn: this.ledActive })
              .onChange((isOn) => {
                if (isOn) {
                  this.turnOnLed();
                } else {
                  this.turnOffLed();
                }
              })
            Text(this.ledActive ? '已开启' : '已关闭').fontSize(16)
          }
          .width('100%')
          .justifyContent(FlexAlign.SpaceBetween)
        }
        .width('90%')
        .padding(16)
        .backgroundColor('#f5f5f5')
        .borderRadius(8)
        .margin({ bottom: 20 })

        // 修改风扇控制UI
        Column() {
          Text('风扇控制').fontSize(20).fontWeight(FontWeight.Medium).margin({ bottom: 10 })

          // 风扇开关
          Row() {
            Text('风扇开关:').fontSize(16)
            Toggle({ type: ToggleType.Switch, isOn: this.fanActive })
              .onChange((isOn) => {
                this.fanActive = isOn;
                this.controlFan(); // 调用风扇控制方法
              })
            Text(this.fanActive ? '已开启' : '已关闭').fontSize(16)
          }
          .width('100%')
          .justifyContent(FlexAlign.SpaceBetween)
          .margin({ bottom: 12 })

          // 风扇方向控制
          Row() {
            Text('风扇方向:').fontSize(16)
            Toggle({ type: ToggleType.Switch, isOn: this.fanDirection })
              .onChange((isOn) => {
                this.fanDirection = isOn;
                if (this.fanActive) {
                  this.controlFan(); // 只有在风扇开启时才调用控制方法
                }
              })
            Text(this.fanDirection ? '正转' : '反转').fontSize(16)
          }
          .width('100%')
          .justifyContent(FlexAlign.SpaceBetween)
          .margin({ bottom: 12 })

          // 风扇速度控制
          Text(`风扇速度: ${this.fanSpeed}%`).fontSize(16).margin({ bottom: 8 })
          Slider({
            value: this.fanSpeed,
            min: 0,
            max: 100,
            step: 1
          })
            .onChange((value: number) => {
              this.fanSpeed = value;
              if (this.fanActive) {
                this.controlFan(); // 只有在风扇开启时才调用控制方法
              }
            })

          Text(`状态: ${this.fanStatus}`).fontSize(16).margin({ top: 10 })
        }
        .width('90%')
        .padding(16)
        .backgroundColor('#f5f5f5')
        .borderRadius(8)
        .margin({ bottom: 20 })

        // 舵机控制
        Column() {
          Text('舵机控制').fontSize(20).fontWeight(FontWeight.Medium).margin({ bottom: 10 })

          Text(`当前角度: ${this.servoAngle}°`).fontSize(16)

          Slider({
            value: this.servoAngle,
            min: 0,
            max: 180,
            step: 1
          })
            .onChange((value: number) => {
              this.servoAngle = value;
              // 直接在onChange中设置舵机角度
              this.setServoAngle(value);
            })
        }
        .width('90%')
        .padding(16)
        .backgroundColor('#f5f5f5')
        .borderRadius(8)
        .margin({ bottom: 20 })

        Column() {
          Text('蜂鸣器:').fontSize(16)
          // 蜂鸣器控制
          Row() {
            Button('蜂鸣器开')
              .onClick(() => {
                this.buzzerOn();
              })
            Button('蜂鸣器关')
              .onClick(() => {
                this.buzzerOff();
              })
          }
          .width('90%')
          .justifyContent(FlexAlign.SpaceBetween)
          .margin({ bottom: 20 })
        }
        .width('90%')
        .padding(16)
        .backgroundColor('#f5f5f5')
        .borderRadius(8)
        .margin({ bottom: 20 })

        // 添加图片展示区域（使用 PixelMap）
        Column() {
          Text('植物实时图像').fontSize(20).fontWeight(FontWeight.Medium).margin({ bottom: 10 })

          if (this.pixelMap) {
            Image(this.pixelMap)
              .width('100%')
              .height(360)
              .objectFit(ImageFit.Contain)
              .borderRadius(8)
              .backgroundColor('#eeeeee')
          } else if (this.imageLoading) {
            Column() {
              Text('图像加载中...')
                .fontSize(16)
            }
            .width('100%')
            .height(360)
            .borderRadius(8)
            .backgroundColor('#eeeeee')
            .justifyContent(FlexAlign.Center)
          } else {
            Column() {
              Text('等待图像...')
                .fontSize(16)
            }
            .width('100%')
            .height(360)
            .borderRadius(8)
            .backgroundColor('#eeeeee')
            .justifyContent(FlexAlign.Center)
          }
          // 拍照功能
          Button('拍摄植物照片')
            .width('90%')
            .onClick(() => {
              this.captureImage();
            })
            .margin({ bottom: 20 })
        }
        .width('90%')
        .padding(16)
        .backgroundColor('#f5f5f5')
        .borderRadius(8)
        .margin({ bottom: 20 })

        // 添加 MQTT 数据上传组件
        MqttSettings({
          soilMoisture: this.soilMoisture,
          lightLevel: this.lightLevel,
          temperature: this.temperature,
          humidity: this.humidity,
          formaldehyde: this.formaldehyde,
          tvoc: this.tvoc,
          co2: this.co2,
          pixelMap: this.pixelMap,
          imageUri: this.imageUri
        })
        .margin({ bottom: 20 })

        // 添加 LLaMA 服务器配置区域
        Column() {
          Text('LLaMA 服务器配置').fontSize(20).fontWeight(FontWeight.Medium).margin({ bottom: 10 })

          Row() {
            Text('服务器地址:').fontSize(16)
            TextInput({ text: this.llamaHost })
              .width('70%')
              .onChange((value) => {
                this.llamaHost = value;
              })
          }.width('100%').justifyContent(FlexAlign.SpaceBetween).margin({ bottom: 8 })

          Row() {
            Text('服务器端口:').fontSize(16)
            TextInput({ text: this.llamaPort })
              .width('70%')
              .onChange((value) => {
                this.llamaPort = value;
              })
          }.width('100%').justifyContent(FlexAlign.SpaceBetween).margin({ bottom: 8 })

          Row() {
            Text('temperature:').fontSize(16)
            TextInput({ text: this.llamaTemperature })
              .width('70%')
              .onChange((value) => {
                this.llamaTemperature = value;
              })
          }.width('100%').justifyContent(FlexAlign.SpaceBetween).margin({ bottom: 8 })

          Row() {
            Text('max_tokens:').fontSize(16)
            TextInput({ text: this.llamaMax_Tokens })
              .width('70%')
              .onChange((value) => {
                this.llamaMax_Tokens = value;
              })
          }.width('100%').justifyContent(FlexAlign.SpaceBetween).margin({ bottom: 8 })

          Button('连接服务器')
            .width('90%')
            .onClick(() => {
              this.connectToLlama();
            })
            .margin({ top: 10 })

          Text(`状态: ${this.llamaConnected ? '已连接' : '未连接'}`)
            .fontSize(16)
            .margin({ top: 10 })
        }
        .width('90%')
        .padding(16)
        .backgroundColor('#f5f5f5')
        .borderRadius(8)
        .margin({ bottom: 20 })

        // 替换原有的 LLaMA 问答区域为新的可滚动历史记录区域
        Column() {
          Text('AI 问答').fontSize(20).fontWeight(FontWeight.Medium).margin({ bottom: 10 })

          // 聊天历史区域
          Column() {
            if (this.chatHistory.length === 0) {
              Text('暂无聊天记录，请发送消息开始对话')
                .fontSize(14)
                .padding(16)
                .textAlign(TextAlign.Center)
            } else {
              Scroll(this.chatScroller) {
                Column() {
                  ForEach(this.chatHistory, (message: ChatMessage, index) => {
                    ChatBubble({ message })
                  })
                }
                .width('100%')
                .padding({ left: 8, right: 8 })
              }
              .scrollBar(BarState.Auto)
              .scrollBarColor(Color.Gray)
              .scrollBarWidth(4)
              .width('100%')
              .height(300)
            }
          }
          .width('100%')
          .height(300)
          .backgroundColor('#F5F5F5')
          .borderRadius(8)
          .margin({ bottom: 12 })

          // 加载状态指示器
          if (this.llamaLoading) {
            Row() {
              LoadingProgress()
                .width(24)
                .height(24)
              Text('AI思考中...')
                .fontSize(14)
                .margin({ left: 8 })
            }
            .margin({ bottom: 10 })
          }

          // 输入和发送区域
          Row() {
            TextInput({ text: this.llamaPrompt, placeholder: '请输入您的问题...' })
              .height(40)
              .width('70%')
              .onChange((value) => {
                this.llamaPrompt = value;
              })
              .enabled(!this.llamaLoading)
              .margin({ right: 8 })

            Button('发送')
              .width('28%')
              .height(40)
              .enabled(!this.llamaLoading && this.llamaConnected)
              .onClick(() => {
                this.askLlama();
              })
          }
          .width('100%')
          .margin({ bottom: 12 })

          // 功能按钮区
          Row() {
            Button({ type: ButtonType.Normal }) {
              Row() {
                // Image($r('app.media.XH')).width(20).height(20).margin({ right: 5 })
                Text('清除历史').fontSize(12)
              }
            }
            .onClick(() => {
              this.clearLlamaHistory();
            })
            .height(36)
          }
          .width('100%')
          .justifyContent(FlexAlign.SpaceBetween)
        }
        .width('90%')
        .padding(16)
        .backgroundColor('#f5f5f5')
        .borderRadius(8)
        .margin({ bottom: 20 })
      }
      .width('100%')
      .padding(16)
      .backgroundColor('#ffffff')
    }
    .width('100%')
    .height('100%')
  }
}