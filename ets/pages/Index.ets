import myproject from '@ohos.myproject';
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
  @State soilTemp: number = 0;
  @State ec: number = 0;
  @State ph: number = 0;
  @State nValue: number = 0;
  @State pValue: number = 0;
  @State kValue: number = 0;
  @State salt: number = 0;
  @State tds: number = 0;
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

  // 自动控制相关状态（由 C++ 常驻线程执行，ETS 仅配置开关和默认阈值）
  @State autoControlEnabled: boolean = false;
  @State autoSoilOn: number = 1200;
  @State autoSoilOff: number = 1600;
  @State autoLightOn: number = 30;
  @State autoLightOff: number = 50;
  @State autoTempOn: number = 30;
  @State autoTempOff: number = 27;
  @State autoCo2On: number = 1200;
  @State autoCo2Off: number = 800;
  @State autoCo2NightOn: number = 1200;
  @State autoCo2NightOff: number = 800;
  // 养分/酸碱报警阈值
  @State autoPhMin: number = 5.5;
  @State autoPhMax: number = 8.5;
  @State autoEcMin: number = 0.0;
  @State autoEcMax: number = 5000.0;
  @State autoNMin: number = 0.0;
  @State autoNMax: number = 3000.0;
  @State autoPMin: number = 0.0;
  @State autoPMax: number = 3000.0;
  @State autoKMin: number = 0.0;
  @State autoKMax: number = 3000.0;
  // 报警状态：0 无告警，1 有告警
  @State nutrientAlarm: number = 0;
  @State autoFanSpeed: number = 80;

  // 添加 LLaMA 相关状态变量
  @State llamaHost: string = '192.168.1.133;
  @State llamaPort: string = '8080';
  @State llamaConnected: boolean = false;
  @State llamaTemperature: string = '0.7';
  @State llamaMax_Tokens: string =  '2048';
  @State llamaSystemMessage: string = "你是一个有用的助手";
  @State llamaPrompt: string = '';
  @State llamaResponse: string = '';
  @State llamaLoading: boolean = false;
  @State llamaIncludeEnvContext: boolean = true;
  @State llamaPlantName: string = '番茄';

  // 添加聊天历史相关变量
  @State chatHistory: ChatMessage[] = []
  chatScroller: Scroller = new Scroller()

  private timer: number = -1;
  private scroller: Scroller = new Scroller();
  private pixelMapTimeout: number = -1;  // 添加超时计时器
  private isPixelMapConverting: boolean = false;  // 添加转换状态跟踪

  aboutToAppear() {
    // 延迟初始化，给系统一些时间准备好硬件接口
    setTimeout(() => {
      this.initHardware();
    }, 1000);

    // 启动定时任务，每2秒更新一次传感器数据
    this.timer = setInterval(() => {
      this.updateSensorData();
    }, 1000);

    // 注册底层图片接收完成回调（替代轮询）
    if (typeof myproject.onImageCaptured === 'function') {
      myproject.onImageCaptured((path: string) => {
        this.imageUri = path;
        this.loadPixelMap(path);
        console.info(`底层回调刷新图片: ${path}`);
      });
    }

    this.clearLlamaHistory();

    // 读取当前自动控制配置
    try {
      this.autoControlEnabled = myproject.getAutoControlEnabled();
      const t = myproject.getAutoControlThresholds();
      this.autoSoilOn = t.soil_on;
      this.autoSoilOff = t.soil_off;
      this.autoLightOn = Math.max(0, Math.min(100, t.light_on));
      this.autoLightOff = Math.max(0, Math.min(100, t.light_off));
      this.autoTempOn = t.temp_on;
      this.autoTempOff = t.temp_off;
      this.autoCo2On = t.co2_on;
      this.autoCo2Off = t.co2_off;
      this.autoCo2NightOn = t.co2_night_on ?? t.co2_on;
      this.autoCo2NightOff = t.co2_night_off ?? t.co2_off;
      this.autoPhMin = t.ph_min;
      this.autoPhMax = t.ph_max;
      this.autoEcMin = t.ec_min;
      this.autoEcMax = t.ec_max;
      this.autoNMin = t.n_min;
      this.autoNMax = t.n_max;
      this.autoPMin = t.p_min;
      this.autoPMax = t.p_max;
      this.autoKMin = t.k_min;
      this.autoKMax = t.k_max;
      this.autoFanSpeed = t.fan_speed;
    } catch (e) {
      console.error(`读取自动控制配置失败: ${e}`);
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
          .then((pixelMap: image.PixelMap) => {
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

    // 取消底层回调
    if (typeof myproject.offImageCaptured === 'function') {
      myproject.offImageCaptured();
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
    const soilData = myproject.getDataByKey('SoilHumi');
    if (soilData !== null) {
      this.soilMoisture = soilData;
    }

    const lightData = myproject.getDataByKey('Light');
    if (lightData !== null) {
      this.lightLevel = lightData;
    }

    const tempData = myproject.getDataByKey('Temp');
    if (tempData !== null) {
      this.temperature = tempData;
    }

    const humiData = myproject.getDataByKey('Humi');
    if (humiData !== null) {
      this.humidity = humiData;
    }

    const ch2oData = myproject.getDataByKey('CH2O');
    if (ch2oData !== null) {
      this.formaldehyde = ch2oData;
    }

    const tvocData = myproject.getDataByKey('TVOC');
    if (tvocData !== null) {
      this.tvoc = tvocData;
    }

    const co2Data = myproject.getDataByKey('CO_2');
    if (co2Data !== null) {
      this.co2 = co2Data;
    }

    // 新增土壤多参数
    const soilTempData = myproject.getDataByKey('SoilTemp');
    if (soilTempData !== null) {
      this.soilTemp = soilTempData;
    }

    const ecData = myproject.getDataByKey('EC');
    if (ecData !== null) {
      this.ec = ecData;
    }

    const phData = myproject.getDataByKey('pH');
    if (phData !== null) {
      this.ph = phData;
    }

    const nData = myproject.getDataByKey('N');
    if (nData !== null) {
      this.nValue = nData;
    }

    const pData = myproject.getDataByKey('P');
    if (pData !== null) {
      this.pValue = pData;
    }

    const kData = myproject.getDataByKey('K');
    if (kData !== null) {
      this.kValue = kData;
    }

    const saltData = myproject.getDataByKey('Salt');
    if (saltData !== null) {
      this.salt = saltData;
    }

    const tdsData = myproject.getDataByKey('TDS');
    if (tdsData !== null) {
      this.tds = tdsData;
    }

    // 依据当前阈值在前端本地计算养分/酸碱报警，和 MQTT alarm 语义保持一致
    const inRangeOrUnset = (value: number, minV: number, maxV: number): boolean => {
      if (minV === 0 && maxV === 0) {
        return true;
      }
      return !(value < minV || value > maxV);
    };

    const phOk = inRangeOrUnset(this.ph, this.autoPhMin, this.autoPhMax);
    const ecOk = inRangeOrUnset(this.ec, this.autoEcMin, this.autoEcMax);
    const nOk = inRangeOrUnset(this.nValue, this.autoNMin, this.autoNMax);
    const pOk = inRangeOrUnset(this.pValue, this.autoPMin, this.autoPMax);
    const kOk = inRangeOrUnset(this.kValue, this.autoKMin, this.autoKMax);

    this.nutrientAlarm = (phOk && ecOk && nOk && pOk && kOk) ? 0 : 1;
  }

  // 水泵控制
  turnOnWaterPump() {
    const result = myproject.pumpOn();
    this.waterPumpActive = (result === 0);
    console.info(`打开水泵: ${result === 0 ? '成功' : '失败'}`);
  }

  turnOffWaterPump() {
    const result = myproject.pumpOff();
    this.waterPumpActive = !(result === 0);
    console.info(`关闭水泵: ${result === 0 ? '成功' : '失败'}`);
  }

  // LED控制
  turnOnLed() {
    const result = myproject.ledOn();
    this.ledActive = (result === 0);
    console.info(`打开LED: ${result === 0 ? '成功' : '失败'}`);
  }

  turnOffLed() {
    const result = myproject.ledOff();
    this.ledActive = !(result === 0);
    console.info(`关闭LED: ${result === 0 ? '成功' : '失败'}`);
  }

  // 更新风扇控制方法
  controlFan() {
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
  }

  // 蜂鸣器控制
  buzzerOn() {
    const result = myproject.buzzeron();
    console.info(`打开蜂鸣器: ${result === 0 ? '成功' : '失败'}`);
  }

  buzzerOff() {
    const result = myproject.buzzeroff();
    console.info(`关闭蜂鸣器: ${result === 0 ? '成功' : '失败'}`);
  }

  // 舵机控制
  setServoAngle(angle: number) {
    // 确保角度在有效范围内
    const validAngle = Math.min(Math.max(angle, 0), 180);
    const result = myproject.setSG90Angle(validAngle);
    console.info(`设置舵机角度 ${validAngle}°: ${result === 0 ? '成功' : '失败'}`);
  }

  // 拍照功能
  captureImage() {
    const result = myproject.sendCapture();
    console.info(`发送拍照指令: ${result === 0 ? '成功' : '失败'}`);
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

    const plantName = this.llamaIncludeEnvContext ? this.llamaPlantName : '';
    myproject.askLlama(currentPrompt, this.llamaIncludeEnvContext, plantName)
      .then((response: string | null) => {
        if (response !== null) {
          // 添加AI回复到历史记录
          this.chatHistory.push(new ChatMessage(response, false));
          // 更新当前显示的回复
          this.llamaResponse = response;
          return;
        }

        // 添加错误消息到历史记录
        this.chatHistory.push(new ChatMessage('没有收到回复', false));
        this.llamaResponse = '没有收到回复';
      })
      .catch((error: Error | string) => {
        console.error(`发送 LLaMA 请求异常: ${error}`);
        // 添加错误消息到历史记录
        this.chatHistory.push(new ChatMessage(`错误: ${error}`, false));
        this.llamaResponse = `错误: ${error}`;
      })
      .finally(() => {
        this.llamaLoading = false;
        // 滚动到最新消息
        this.scrollToBottom();
      });
  }

  // 滚动到聊天记录底部
  scrollToBottom() {
    this.chatScroller.scrollToIndex(this.chatHistory.length - 1);
  }

  // 修改清除LLaMA历史记录的方法
  clearLlamaHistory() {
    const result = myproject.clearLlamaHistory();
    console.info(`清除LLaMA对话历史: ${result === 0 ? '成功' : '失败'}`);

    // 清除UI界面上的历史记录
    this.chatHistory = [];
    // 清除当前界面上的响应
    this.llamaResponse = '';

    // 提示用户
    prompt.showToast({
      message: result === 0 ? '对话历史已清除' : '清除对话历史失败',
      duration: 2000,
    });
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
            Text(`${this.lightLevel}%`).fontSize(16)
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

          // 新增土壤与养分参数展示
          Row() {
            Text('土壤温度:').fontSize(16)
            Text(`${this.soilTemp}°C`).fontSize(16)
          }.width('100%').justifyContent(FlexAlign.SpaceBetween).margin({ bottom: 8 })

          Row() {
            Text('EC:').fontSize(16)
            Text(`${this.ec} mS/cm`).fontSize(16)
          }.width('100%').justifyContent(FlexAlign.SpaceBetween).margin({ bottom: 8 })

          Row() {
            Text('pH:').fontSize(16)
            Text(`${this.ph}`).fontSize(16)
          }.width('100%').justifyContent(FlexAlign.SpaceBetween).margin({ bottom: 8 })

          Row() {
            Text('氮(N):').fontSize(16)
            Text(`${this.nValue} mg/kg`).fontSize(16)
          }.width('100%').justifyContent(FlexAlign.SpaceBetween).margin({ bottom: 8 })

          Row() {
            Text('磷(P):').fontSize(16)
            Text(`${this.pValue} mg/kg`).fontSize(16)
          }.width('100%').justifyContent(FlexAlign.SpaceBetween).margin({ bottom: 8 })

          Row() {
            Text('钾(K):').fontSize(16)
            Text(`${this.kValue} mg/kg`).fontSize(16)
          }.width('100%').justifyContent(FlexAlign.SpaceBetween).margin({ bottom: 8 })

          Row() {
            Text('盐分:').fontSize(16)
            Text(`${this.salt} mg/kg`).fontSize(16)
          }.width('100%').justifyContent(FlexAlign.SpaceBetween).margin({ bottom: 8 })

          Row() {
            Text('TDS:').fontSize(16)
            Text(`${this.tds} ppm`).fontSize(16)
          }.width('100%').justifyContent(FlexAlign.SpaceBetween).margin({ bottom: 8 })

          Row() {
            Text('养分/酸碱报警:').fontSize(16)
            Text(this.nutrientAlarm === 0 ? '正常' : '超出阈值')
              .fontSize(16)
              .fontColor(this.nutrientAlarm === 0 ? Color.Black : Color.Red)
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

        // 自动控制配置（调用 C++ 线程中的逻辑）
        Column() {
          Text('自动控制（设备侧阈值）').fontSize(20).fontWeight(FontWeight.Medium).margin({ bottom: 10 })

          Row() {
            Text('启用自动控制:').fontSize(16)
            Toggle({ type: ToggleType.Switch, isOn: this.autoControlEnabled })
              .onChange((isOn: boolean) => {
                this.autoControlEnabled = isOn;
                myproject.setAutoControlEnabled(isOn);
              })
          }
          .width('100%')
          .justifyContent(FlexAlign.SpaceBetween)
          .margin({ bottom: 12 })

          // 阈值调节：土壤湿度
          Text(`土壤干阈值 soil_on: ${this.autoSoilOn}`).fontSize(14).margin({ bottom: 4 })
          Slider({
            value: this.autoSoilOn,
            min: 0,
            max: 3000,
            step: 10
          })
            .onChange((value: number) => {
              this.autoSoilOn = value;
            })
            .margin({ bottom: 8 })

          Text(`土壤湿阈值 soil_off: ${this.autoSoilOff}`).fontSize(14).margin({ bottom: 4 })
          Slider({
            value: this.autoSoilOff,
            min: 0,
            max: 3000,
            step: 10
          })
            .onChange((value: number) => {
              this.autoSoilOff = value;
            })
            .margin({ bottom: 8 })

          // 阈值调节：光照
          Text(`光照暗阈值 light_on: ${this.autoLightOn}`).fontSize(14).margin({ bottom: 4 })
          Slider({
            value: this.autoLightOn,
            min: 0,
            max: 100,
            step: 1
          })
            .onChange((value: number) => {
              this.autoLightOn = value;
            })
            .margin({ bottom: 8 })

          Text(`光照亮阈值 light_off: ${this.autoLightOff}`).fontSize(14).margin({ bottom: 4 })
          Slider({
            value: this.autoLightOff,
            min: 0,
            max: 100,
            step: 1
          })
            .onChange((value: number) => {
              this.autoLightOff = value;
            })
            .margin({ bottom: 8 })

          // 阈值调节：温度（使用文本输入，支持小数）
          Row() {
            Text('温度高阈值 temp_on (°C):').fontSize(14)
            TextInput({ text: `${this.autoTempOn}` })
              .width('40%')
              .onChange((v: string) => {
                const n = Number.parseFloat(v);
                if (!Number.isNaN(n)) {
                  this.autoTempOn = n;
                }
              })
          }
          .width('100%')
          .justifyContent(FlexAlign.SpaceBetween)
          .margin({ bottom: 4 })

          Row() {
            Text('温度低阈值 temp_off (°C):').fontSize(14)
            TextInput({ text: `${this.autoTempOff}` })
              .width('40%')
              .onChange((v: string) => {
                const n = Number.parseFloat(v);
                if (!Number.isNaN(n)) {
                  this.autoTempOff = n;
                }
              })
          }
          .width('100%')
          .justifyContent(FlexAlign.SpaceBetween)
          .margin({ bottom: 8 })

          // 阈值调节：二氧化碳 CO2（白天）
          Row() {
            Text('CO2 白天高阈值 co2_on (ppm):').fontSize(14)
            TextInput({ text: `${this.autoCo2On}` })
              .width('40%')
              .onChange((v: string) => {
                const n = Number.parseFloat(v);
                if (!Number.isNaN(n)) {
                  this.autoCo2On = n;
                }
              })
          }
          .width('100%')
          .justifyContent(FlexAlign.SpaceBetween)
          .margin({ bottom: 4 })

          Row() {
            Text('CO2 白天低阈值 co2_off (ppm):').fontSize(14)
            TextInput({ text: `${this.autoCo2Off}` })
              .width('40%')
              .onChange((v: string) => {
                const n = Number.parseFloat(v);
                if (!Number.isNaN(n)) {
                  this.autoCo2Off = n;
                }
              })
          }
          .width('100%')
          .justifyContent(FlexAlign.SpaceBetween)
          .margin({ bottom: 8 })

          // 阈值调节：二氧化碳 CO2（夜间）
          Row() {
            Text('CO2 夜间高阈值 co2_night_on (ppm):').fontSize(14)
            TextInput({ text: `${this.autoCo2NightOn}` })
              .width('40%')
              .onChange((v: string) => {
                const n = Number.parseFloat(v);
                if (!Number.isNaN(n)) {
                  this.autoCo2NightOn = n;
                }
              })
          }
          .width('100%')
          .justifyContent(FlexAlign.SpaceBetween)
          .margin({ bottom: 4 })

          Row() {
            Text('CO2 夜间低阈值 co2_night_off (ppm):').fontSize(14)
            TextInput({ text: `${this.autoCo2NightOff}` })
              .width('40%')
              .onChange((v: string) => {
                const n = Number.parseFloat(v);
                if (!Number.isNaN(n)) {
                  this.autoCo2NightOff = n;
                }
              })
          }
          .width('100%')
          .justifyContent(FlexAlign.SpaceBetween)
          .margin({ bottom: 8 })

          // 阈值调节：土壤多参数/酸碱（用于报警，不直接驱动执行器）
          Text('土壤多参数/酸碱报警阈值').fontSize(16).margin({ top: 8, bottom: 4 })

          Row() {
            Text('pH 最小值 ph_min:').fontSize(14)
            TextInput({ text: `${this.autoPhMin}` })
              .width('35%')
              .onChange((v: string) => {
                const n = Number.parseFloat(v);
                if (!Number.isNaN(n)) {
                  this.autoPhMin = n;
                }
              })
            Text('最大值 ph_max:').fontSize(14)
            TextInput({ text: `${this.autoPhMax}` })
              .width('35%')
              .onChange((v: string) => {
                const n = Number.parseFloat(v);
                if (!Number.isNaN(n)) {
                  this.autoPhMax = n;
                }
              })
          }
          .width('100%')
          .justifyContent(FlexAlign.SpaceBetween)
          .margin({ bottom: 4 })

          Row() {
            Text('EC 最小值 ec_min:').fontSize(14)
            TextInput({ text: `${this.autoEcMin}` })
              .width('35%')
              .onChange((v: string) => {
                const n = Number.parseFloat(v);
                if (!Number.isNaN(n)) {
                  this.autoEcMin = n;
                }
              })
            Text('最大值 ec_max:').fontSize(14)
            TextInput({ text: `${this.autoEcMax}` })
              .width('35%')
              .onChange((v: string) => {
                const n = Number.parseFloat(v);
                if (!Number.isNaN(n)) {
                  this.autoEcMax = n;
                }
              })
          }
          .width('100%')
          .justifyContent(FlexAlign.SpaceBetween)
          .margin({ bottom: 4 })

          Row() {
            Text('氮 N 最小值 n_min:').fontSize(14)
            TextInput({ text: `${this.autoNMin}` })
              .width('35%')
              .onChange((v: string) => {
                const n = Number.parseFloat(v);
                if (!Number.isNaN(n)) {
                  this.autoNMin = n;
                }
              })
            Text('最大值 n_max:').fontSize(14)
            TextInput({ text: `${this.autoNMax}` })
              .width('35%')
              .onChange((v: string) => {
                const n = Number.parseFloat(v);
                if (!Number.isNaN(n)) {
                  this.autoNMax = n;
                }
              })
          }
          .width('100%')
          .justifyContent(FlexAlign.SpaceBetween)
          .margin({ bottom: 4 })

          Row() {
            Text('磷 P 最小值 p_min:').fontSize(14)
            TextInput({ text: `${this.autoPMin}` })
              .width('35%')
              .onChange((v: string) => {
                const n = Number.parseFloat(v);
                if (!Number.isNaN(n)) {
                  this.autoPMin = n;
                }
              })
            Text('最大值 p_max:').fontSize(14)
            TextInput({ text: `${this.autoPMax}` })
              .width('35%')
              .onChange((v: string) => {
                const n = Number.parseFloat(v);
                if (!Number.isNaN(n)) {
                  this.autoPMax = n;
                }
              })
          }
          .width('100%')
          .justifyContent(FlexAlign.SpaceBetween)
          .margin({ bottom: 4 })

          Row() {
            Text('钾 K 最小值 k_min:').fontSize(14)
            TextInput({ text: `${this.autoKMin}` })
              .width('35%')
              .onChange((v: string) => {
                const n = Number.parseFloat(v);
                if (!Number.isNaN(n)) {
                  this.autoKMin = n;
                }
              })
            Text('最大值 k_max:').fontSize(14)
            TextInput({ text: `${this.autoKMax}` })
              .width('35%')
              .onChange((v: string) => {
                const n = Number.parseFloat(v);
                if (!Number.isNaN(n)) {
                  this.autoKMax = n;
                }
              })
          }
          .width('100%')
          .justifyContent(FlexAlign.SpaceBetween)
          .margin({ bottom: 8 })

          // 阈值调节：风扇速度
          Text(`风扇速度 fan_speed: ${this.autoFanSpeed}%`).fontSize(14).margin({ bottom: 4 })
          Slider({
            value: this.autoFanSpeed,
            min: 0,
            max: 100,
            step: 1
          })
            .onChange((value: number) => {
              this.autoFanSpeed = value;
            })
            .margin({ bottom: 12 })

          // 只提供一个简单入口：将当前 ETS 中的默认值一次性同步到底层
          Button('将当前默认阈值同步到设备')
            .width('100%')
            .onClick(() => {
              myproject.setAutoControlThresholds({
                soil_on: this.autoSoilOn,
                soil_off: this.autoSoilOff,
                light_on: this.autoLightOn,
                light_off: this.autoLightOff,
                temp_on: this.autoTempOn,
                temp_off: this.autoTempOff,
                co2_on: this.autoCo2On,
                co2_off: this.autoCo2Off,
                co2_night_on: this.autoCo2NightOn,
                co2_night_off: this.autoCo2NightOff,
                ph_min: this.autoPhMin,
                ph_max: this.autoPhMax,
                ec_min: this.autoEcMin,
                ec_max: this.autoEcMax,
                n_min: this.autoNMin,
                n_max: this.autoNMax,
                p_min: this.autoPMin,
                p_max: this.autoPMax,
                k_min: this.autoKMin,
                k_max: this.autoKMax,
                fan_speed: this.autoFanSpeed,
              });
              prompt.showToast({ message: '阈值已同步到设备', duration: 2000 });
            })
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

          Row() {
            Text('提供环境信息:').fontSize(16)
            Toggle({ type: ToggleType.Switch, isOn: this.llamaIncludeEnvContext })
              .onChange((isOn) => {
                this.llamaIncludeEnvContext = isOn;
              })
              .enabled(!this.llamaLoading)
          }.width('100%').justifyContent(FlexAlign.SpaceBetween).margin({ bottom: 8 })

          Row() {
            Text('植物名字:').fontSize(16)
            TextInput({ text: this.llamaPlantName, placeholder: '例如：番茄/薄荷/草莓' })
              .width('70%')
              .onChange((value) => {
                this.llamaPlantName = value;
              })
              .enabled(!this.llamaLoading && this.llamaIncludeEnvContext)
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