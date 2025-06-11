#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "mqttconfigdialog.h"
#include <QTimer>      // 添加 QTimer 头文件
#include <QMessageBox> // 添加 QMessageBox 头文件
#include <QDateTime>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPen>
#include <QValueAxis>
#include <QDateTimeAxis>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    setupUI();

    // 显示MQTT配置对话框
    QTimer::singleShot(0, this, &MainWindow::showMqttConfigDialog);
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::setupUI()
{
    // 创建主标签页
    tabWidget = new QTabWidget(this);

    // 创建三个页面
    dataPage = new QWidget();
    chartPage = new QWidget();
    imagePage = new QWidget();

    // 配置数据页面
    QVBoxLayout *dataLayout = new QVBoxLayout(dataPage);

    // 创建网格布局，用于传感器卡片
    QGridLayout *sensorsGrid = new QGridLayout();
    sensorsGrid->setSpacing(20);

    // 创建传感器卡片
    labelDeviceId = createSensorCard("🔌 设备ID", "--");
    labelTimestamp = createSensorCard("🕕 时间戳", "--");
    labelSoilMoisture = createSensorCard("🌱 土壤湿度", "--");
    labelLightLevel = createSensorCard("☀️ 光照强度", "--");
    labelTemperature = createSensorCard("🌡️ 温度", "--");
    labelHumidity = createSensorCard("💧 湿度", "--");
    labelFormaldehyde = createSensorCard("🧪 甲醛", "--");
    labelTvoc = createSensorCard("🏭 TVOC", "--");
    labelCo2 = createSensorCard("🌎 CO₂", "--");

    // 添加传感器卡片到网格
    sensorsGrid->addWidget(labelDeviceId, 0, 0);
    sensorsGrid->addWidget(labelTimestamp, 0, 1);
    sensorsGrid->addWidget(labelTemperature, 1, 0);
    sensorsGrid->addWidget(labelHumidity, 1, 1);
    sensorsGrid->addWidget(labelSoilMoisture, 2, 0);
    sensorsGrid->addWidget(labelLightLevel, 2, 1);
    sensorsGrid->addWidget(labelFormaldehyde, 3, 0);
    sensorsGrid->addWidget(labelTvoc, 3, 1);
    sensorsGrid->addWidget(labelCo2, 4, 0);

    dataLayout->addLayout(sensorsGrid);
    dataLayout->addStretch();

    // 设置图表页面
    setupChartPage();

    // 设置图像页面
    setupImagePage();

    // 添加页面到标签页
    tabWidget->addTab(dataPage, "数据面板");
    tabWidget->addTab(chartPage, "图表");
    tabWidget->addTab(imagePage, "图像");

    // 设置中央窗口部件
    setCentralWidget(tabWidget);

    // 应用样式
    this->setStyleSheet(R"(
        QMainWindow {
            background-color: #f0f0f0;
        }
        QTabWidget::pane {
            border: 1px solid #cccccc;
            background: white;
            border-radius: 5px;
        }
        QTabBar::tab {
            background: #e0e0e0;
            border: 1px solid #c0c0c0;
            border-bottom: none;
            border-top-left-radius: 4px;
            border-top-right-radius: 4px;
            padding: 8px 15px;
        }
        QTabBar::tab:selected {
            background: white;
            margin-bottom: -1px;
        }
        QLabel {
            padding: 10px;
            background-color: white;
            border-radius: 10px;
            border: 1px solid #e0e0e0;
        }
    )");
}

void MainWindow::setupChartPage()
{
    // 创建垂直布局作为主布局
    QVBoxLayout *chartLayout = new QVBoxLayout(chartPage);

    // 创建滚动区域
    QScrollArea *scrollArea = new QScrollArea();
    scrollArea->setWidgetResizable(true);
    scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);

    // 创建一个容器widget来放置图表
    QWidget *chartsContainer = new QWidget();
    QVBoxLayout *containerLayout = new QVBoxLayout(chartsContainer);

    // 创建网格布局，每行1个图表
    QGridLayout *chartsGrid = new QGridLayout();
    chartsGrid->setSpacing(20);

    // 温度图表
    temperatureChart = new QChart();
    temperatureChart->setTitle("温度变化 (°C)");
    temperatureChart->legend()->hide();

    temperatureSeries = new QLineSeries();
    temperatureChart->addSeries(temperatureSeries);

    // 湿度图表
    humidityChart = new QChart();
    humidityChart->setTitle("湿度变化 (%)");
    humidityChart->legend()->hide();

    humiditySeries = new QLineSeries();
    humidityChart->addSeries(humiditySeries);

    // 土壤湿度图表
    soilMoistureChart = new QChart();
    soilMoistureChart->setTitle("土壤湿度变化");
    soilMoistureChart->legend()->hide();

    soilMoistureSeries = new QLineSeries();
    soilMoistureChart->addSeries(soilMoistureSeries);

    // 光照强度图表
    lightLevelChart = new QChart();
    lightLevelChart->setTitle("光照强度变化");
    lightLevelChart->legend()->hide();

    lightLevelSeries = new QLineSeries();
    lightLevelChart->addSeries(lightLevelSeries);

    // CO2 图表
    co2Chart = new QChart();
    co2Chart->setTitle("CO₂ 变化 (ppm)");
    co2Chart->legend()->hide();

    co2Series = new QLineSeries();
    co2Chart->addSeries(co2Series);

    // TVOC 图表
    tvocChart = new QChart();
    tvocChart->setTitle("TVOC 变化 (ppm)");
    tvocChart->legend()->hide();

    tvocSeries = new QLineSeries();
    tvocChart->addSeries(tvocSeries);

    // 甲醛图表
    formaldehydeChart = new QChart();
    formaldehydeChart->setTitle("甲醛变化 (ppm)");
    formaldehydeChart->legend()->hide();

    formaldehydeSeries = new QLineSeries();
    formaldehydeChart->addSeries(formaldehydeSeries);

    // 创建图表视图
    temperatureChartView = new QChartView(temperatureChart);
    temperatureChartView->setRenderHint(QPainter::Antialiasing);

    humidityChartView = new QChartView(humidityChart);
    humidityChartView->setRenderHint(QPainter::Antialiasing);

    soilMoistureChartView = new QChartView(soilMoistureChart);
    soilMoistureChartView->setRenderHint(QPainter::Antialiasing);

    lightLevelChartView = new QChartView(lightLevelChart);
    lightLevelChartView->setRenderHint(QPainter::Antialiasing);

    // 创建新图表的视图
    co2ChartView = new QChartView(co2Chart);
    co2ChartView->setRenderHint(QPainter::Antialiasing);

    tvocChartView = new QChartView(tvocChart);
    tvocChartView->setRenderHint(QPainter::Antialiasing);

    formaldehydeChartView = new QChartView(formaldehydeChart);
    formaldehydeChartView->setRenderHint(QPainter::Antialiasing);

    // 设置每个图表视图的最小高度，以确保良好的可视性
    QList<QChartView*> allChartViews;
    allChartViews << temperatureChartView << humidityChartView
                  << soilMoistureChartView << lightLevelChartView
                  << co2ChartView << tvocChartView << formaldehydeChartView;

    for (QChartView *view : allChartViews) {
        view->setMinimumHeight(300); // 增加高度，因为现在每行只有一个图表
    }

    // 每行1个图表添加到网格中
    chartsGrid->addWidget(temperatureChartView, 0, 0);
    chartsGrid->addWidget(humidityChartView, 1, 0);
    chartsGrid->addWidget(soilMoistureChartView, 2, 0);
    chartsGrid->addWidget(lightLevelChartView, 3, 0);
    chartsGrid->addWidget(co2ChartView, 4, 0);
    chartsGrid->addWidget(tvocChartView, 5, 0);
    chartsGrid->addWidget(formaldehydeChartView, 6, 0);

    // 添加网格布局到容器的垂直布局
    containerLayout->addLayout(chartsGrid);
    containerLayout->addStretch();

    // 将容器设置为滚动区域的widget
    scrollArea->setWidget(chartsContainer);

    // 添加滚动区域到主布局
    chartLayout->addWidget(scrollArea);

    // 初始化所有图表的坐标轴
    QDateTime now = QDateTime::currentDateTime();

    QList<QChart*> charts;
    charts.append(temperatureChart);
    charts.append(humidityChart);
    charts.append(soilMoistureChart);
    charts.append(lightLevelChart);
    charts.append(co2Chart);
    charts.append(tvocChart);
    charts.append(formaldehydeChart);

    // 更新图表列表以包括新图表
    for (QChart *chart : charts) {
        // 创建X轴（时间轴）
        QDateTimeAxis *axisX = new QDateTimeAxis();
        axisX->setFormat("hh:mm:ss");
        axisX->setTitleText("时间");
        axisX->setRange(now.addSecs(-60), now);

        // 创建Y轴
        QValueAxis *axisY = new QValueAxis();
        axisY->setLabelFormat("%.2f"); // 修改为显示小数点后2位
        // 删除Y轴标题文本
        axisY->setTitleText("");

        // 为图表添加坐标轴
        chart->addAxis(axisX, Qt::AlignBottom);
        chart->addAxis(axisY, Qt::AlignLeft);

        // 将系列附加到坐标轴
        if (!chart->series().isEmpty()) {
            QAbstractSeries *series = chart->series().first();
            series->attachAxis(axisX);
            series->attachAxis(axisY);

            // 设置线条样式
            QPen seriesPen;
            seriesPen.setWidth(2);
            seriesPen.setColor(QColor(33, 150, 243));

            // 使用dynamic_cast转换为QLineSeries后设置pen
            QLineSeries *lineSeries = dynamic_cast<QLineSeries*>(series);
            if (lineSeries) {
                lineSeries->setPen(seriesPen);
            }
        }
    }
}

void MainWindow::setupImagePage()
{
    QVBoxLayout *imageLayout = new QVBoxLayout(imagePage);

    // 创建滚动区域，以便图像太大时可以滚动查看
    QScrollArea *scrollArea = new QScrollArea();
    scrollArea->setWidgetResizable(true);
    scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);

    // 创建图像标签
    imageLabel = new QLabel(scrollArea);
    imageLabel->setAlignment(Qt::AlignCenter);
    imageLabel->setText("等待图像数据...");
    imageLabel->setStyleSheet("background-color: #f0f0f0; padding: 20px;");

    scrollArea->setWidget(imageLabel);
    imageLayout->addWidget(scrollArea);
}

void MainWindow::showMqttConfigDialog()
{
    MqttConfigDialog dialog(this);
    
    // 确保对话框居中显示
    dialog.setModal(true);
    
#ifdef Q_OS_ANDROID
    // 在Android上延迟显示对话框，以确保UI已完全初始化
    QTimer::singleShot(300, [&dialog]() {
        dialog.show();
        dialog.raise();
        dialog.activateWindow();
    });
#endif

    if (dialog.exec() == QDialog::Accepted) {
        mqttBrokerAddress = dialog.brokerAddress();
        mqttBrokerPort = dialog.brokerPort();
        mqttDataTopic = dialog.dataTopicName();
        mqttImageTopic = dialog.imageTopicName();

        // 如果所有必填字段都已填写，则连接MQTT
        if (!mqttBrokerAddress.isEmpty() && !mqttDataTopic.isEmpty()) {
            connectMQTT();
        } else {
            QMessageBox::warning(this, "配置不完整", "MQTT服务器地址和数据主题是必填的。");
        }
    } else {
        // 用户取消了，可以退出应用或使用默认值
        QMessageBox::information(this, "MQTT连接", "未配置MQTT连接，将使用默认设置或无法接收数据。");
    }
}

void MainWindow::connectMQTT()
{
    mqttClient = new QMqttClient(this);
    mqttClient->setHostname(mqttBrokerAddress);
    mqttClient->setPort(mqttBrokerPort);

    // 连接信号槽
    connect(mqttClient, &QMqttClient::connected, this, &MainWindow::onMQTTConnected);
    connect(mqttClient, &QMqttClient::messageReceived, this, &MainWindow::onMQTTMessageReceived);

    // 连接到MQTT代理
    mqttClient->connectToHost();
}

void MainWindow::onMQTTConnected()
{
    qDebug() << "已连接到MQTT代理";

    // 订阅数据主题
    QMqttSubscription *dataSubscription = mqttClient->subscribe(QMqttTopicFilter(mqttDataTopic));
    if (!dataSubscription) {
        qDebug() << "订阅数据主题失败";
    } else {
        qDebug() << "成功订阅数据主题:" << mqttDataTopic;
    }

    // 如果提供了图片主题，则订阅
    if (!mqttImageTopic.isEmpty()) {
        QMqttSubscription *imageSubscription = mqttClient->subscribe(QMqttTopicFilter(mqttImageTopic));
        if (!imageSubscription) {
            qDebug() << "订阅图片主题失败";
        } else {
            qDebug() << "成功订阅图片主题:" << mqttImageTopic;
        }
    }
}

void MainWindow::onMQTTMessageReceived(const QByteArray &message, const QMqttTopicName &topic)
{
    // 检查主题并处理消息
    QString topicStr = topic.name();

    if (!mqttImageTopic.isEmpty() && topicStr == mqttImageTopic) {
        // 处理图像数据
        try {
            // 首先尝试将接收到的消息解码为Base64格式
            QByteArray imageData = QByteArray::fromBase64(message);
            
            QImage image;
            int maxWidth = imageLabel->width() - 40; // 减去padding

            if (image.loadFromData(imageData)) {
                if (image.width() > maxWidth) {
                    image = image.scaledToWidth(maxWidth, Qt::SmoothTransformation);
                }
                imageLabel->setPixmap(QPixmap::fromImage(image));
            } else {
                // 如果解码失败，尝试直接使用原始数据（向后兼容）
                if (image.loadFromData(message)) {
                    if (image.width() > maxWidth) {
                        image = image.scaledToWidth(maxWidth, Qt::SmoothTransformation);
                    }
                    imageLabel->setPixmap(QPixmap::fromImage(image));
                } else {
                    imageLabel->setText("图像解码失败");
                    qDebug() << "图像解码失败：无法从Base64数据或原始数据加载图像";
                }
            }
        } catch (const std::exception& e) {
            qDebug() << "处理图像数据时出错: " << e.what();
            imageLabel->setText("图像处理错误");
        } catch (...) {
            qDebug() << "处理图像数据时出现未知错误";
            imageLabel->setText("图像处理错误");
        }
    } else if (topicStr == mqttDataTopic) {
        // 处理传感器数据
        try {
            QJsonDocument doc = QJsonDocument::fromJson(message);
            if (doc.isNull() || !doc.isObject()) {
                qDebug() << "JSON解析失败";
                return;
            }

            QJsonObject obj = doc.object();

            // 提取顶层属性
            QString deviceId = obj["deviceId"].toString();
            QString timestamp = obj["timestamp"].toString();
            
            // 检查sensors对象是否存在
            if (!obj.contains("sensors") || !obj["sensors"].isObject()) {
                qDebug() << "JSON格式错误: 没有找到sensors对象";
                return;
            }
            
            // 获取sensors对象
            QJsonObject sensors = obj["sensors"].toObject();
            
            // 从sensors对象中提取数据
            int soilMoisture = sensors["soilMoisture"].toInt();
            int lightLevel = sensors["lightLevel"].toInt();
            double temperature = sensors["temperature"].toDouble();
            int humidity = sensors["humidity"].toInt();
            double formaldehyde = sensors["formaldehyde"].toDouble();
            double tvoc = sensors["tvoc"].toDouble();
            int co2 = sensors["co2"].toInt();

            // 更新UI
            labelDeviceId->setText(QString("<b>🔌 设备ID:</b><br><span style='font-size:16px;'>%1</span>").arg(deviceId));
            labelTimestamp->setText(QString("<b>🕕 时间戳:</b><br><span style='font-size:16px;'>%1</span>").arg(timestamp));
            labelSoilMoisture->setText(QString("<b>🌱 土壤湿度:</b><br><span style='font-size:16px;'>%1</span>").arg(soilMoisture));
            labelLightLevel->setText(QString("<b>☀️ 光照强度:</b><br><span style='font-size:16px;'>%1</span>").arg(lightLevel));
            labelTemperature->setText(QString("<b>🌡️ 温度:</b><br><span style='font-size:16px;'>%1 °C</span>").arg(temperature));
            labelHumidity->setText(QString("<b>💧 湿度:</b><br><span style='font-size:16px;'>%1 %</span>").arg(humidity));
            labelFormaldehyde->setText(QString("<b>🧪 甲醛:</b><br><span style='font-size:16px;'>%1 ppm</span>").arg(formaldehyde));
            labelTvoc->setText(QString("<b>🏭 TVOC:</b><br><span style='font-size:16px;'>%1 ppm</span>").arg(tvoc));
            labelCo2->setText(QString("<b>🌎 CO₂:</b><br><span style='font-size:16px;'>%1 ppm</span>").arg(co2));

            // 更新图表
            QDateTime dateTime = QDateTime::fromString(timestamp, Qt::ISODate);
            updateCharts(dateTime, temperature, humidity, soilMoisture, lightLevel, formaldehyde, tvoc, co2);
        } catch (...) {
            qDebug() << "处理传感器数据时出错";
        }
    }
}

void MainWindow::updateCharts(const QDateTime &timestamp, double temperature, int humidity, int soilMoisture,
                             int lightLevel, double formaldehyde, double tvoc, int co2)
{
    qint64 timeMs = timestamp.toMSecsSinceEpoch();

    // 添加数据点到各个系列
    temperatureSeries->append(timeMs, temperature);
    humiditySeries->append(timeMs, humidity);
    soilMoistureSeries->append(timeMs, soilMoisture);
    lightLevelSeries->append(timeMs, lightLevel);
    co2Series->append(timeMs, co2);                      // 新增 CO2 数据
    tvocSeries->append(timeMs, tvoc);                    // 新增 TVOC 数据
    formaldehydeSeries->append(timeMs, formaldehyde);    // 新增甲醛数据

    // 保存时间点以便管理数据量
    timeQueue.enqueue(timestamp);

    // 限制数据点数量
    if (timeQueue.size() > MAX_DATA_POINTS) {
        QDateTime oldestTime = timeQueue.dequeue();
        qint64 oldestTimeMs = oldestTime.toMSecsSinceEpoch();

        // 移除旧数据点
        for (int i = 0; i < temperatureSeries->count(); i++) {
            if (temperatureSeries->at(i).x() <= oldestTimeMs) {
                temperatureSeries->remove(i);
                break;
            }
        }

        for (int i = 0; i < humiditySeries->count(); i++) {
            if (humiditySeries->at(i).x() <= oldestTimeMs) {
                humiditySeries->remove(i);
                break;
            }
        }

        for (int i = 0; i < soilMoistureSeries->count(); i++) {
            if (soilMoistureSeries->at(i).x() <= oldestTimeMs) {
                soilMoistureSeries->remove(i);
                break;
            }
        }

        for (int i = 0; i < lightLevelSeries->count(); i++) {
            if (lightLevelSeries->at(i).x() <= oldestTimeMs) {
                lightLevelSeries->remove(i);
                break;
            }
        }
    }

    // 更新所有图表的X轴范围
    if (!timeQueue.isEmpty()) {
        QDateTime newest = timeQueue.last();
        QDateTime oldest = timeQueue.first();

        // 更新图表列表以包括新图表
        QList<QChart*> charts;
        charts.append(temperatureChart);
        charts.append(humidityChart);
        charts.append(soilMoistureChart);
        charts.append(lightLevelChart);
        charts.append(co2Chart);
        charts.append(tvocChart);
        charts.append(formaldehydeChart);

        for (QChart *chart : charts) {
            QDateTimeAxis *axisX = qobject_cast<QDateTimeAxis*>(chart->axes(Qt::Horizontal).first());
            if (axisX) {
                axisX->setRange(oldest, newest.addSecs(5)); // 添加5秒的缓冲
            }

            // 更新Y轴范围（可选）
            QValueAxis *axisY = qobject_cast<QValueAxis*>(chart->axes(Qt::Vertical).first());
            if (axisY && !chart->series().isEmpty()) {
                QLineSeries *series = qobject_cast<QLineSeries*>(chart->series().first());
                if (series && series->count() > 0) {
                    qreal minY = std::numeric_limits<qreal>::max();
                    qreal maxY = std::numeric_limits<qreal>::min();

                    for (int i = 0; i < series->count(); ++i) {
                        qreal y = series->at(i).y();
                        minY = qMin(minY, y);
                        maxY = qMax(maxY, y);
                    }

                    // 添加一点余量
                    qreal margin = (maxY - minY) * 0.1;
                    if (margin == 0) margin = 1;

                    axisY->setRange(minY - margin, maxY + margin);
                }
            }
        }
    }
}

QLabel* MainWindow::createSensorCard(const QString &title, const QString &value)
{
    QLabel *label = new QLabel(QString("<b>%1:</b><br><span style='font-size:16px;'>%2</span>").arg(title).arg(value));
    label->setAlignment(Qt::AlignCenter);
    label->setMinimumSize(200, 100);
    return label;
}
