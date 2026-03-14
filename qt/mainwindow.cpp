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
#include <QCheckBox>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QScrollArea>

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
    // 顶部工具栏（包含 MQTT 连接信息和重连按钮）
    setupTopToolbar();

    // 创建主标签页
    tabWidget = new QTabWidget(this);

    // 创建页面
    dataPage = new QWidget();
    chartPage = new QWidget();
    imagePage = new QWidget();
    autoControlPage = new QWidget();
    manualControlPage = new QWidget();

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
    labelSoilTemp = createSensorCard("🌡️ 土壤温度", "--");
    labelEc = createSensorCard("⚡ EC", "--");
    labelPh = createSensorCard("📏 pH", "--");
    labelN = createSensorCard("🧪 氮(N)", "--");
    labelP = createSensorCard("🧪 磷(P)", "--");
    labelK = createSensorCard("🧪 钾(K)", "--");
    labelSalt = createSensorCard("🧂 盐分", "--");
    labelTds = createSensorCard("💧 TDS", "--");

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
    sensorsGrid->addWidget(labelSoilTemp, 4, 1);
    sensorsGrid->addWidget(labelEc, 5, 0);
    sensorsGrid->addWidget(labelPh, 5, 1);
    sensorsGrid->addWidget(labelN, 6, 0);
    sensorsGrid->addWidget(labelP, 6, 1);
    sensorsGrid->addWidget(labelK, 7, 0);
    sensorsGrid->addWidget(labelSalt, 7, 1);
    sensorsGrid->addWidget(labelTds, 8, 0);

    dataLayout->addLayout(sensorsGrid);
    dataLayout->addStretch();

    // 设置图表页面
    setupChartPage();

    // 设置图像页面
    setupImagePage();

    // 设置自动控制页面
    setupAutoControlPage();

    // 设置手动控制页面
    setupManualControlPage();

    // 为数据页面创建滚动区域，使内容过长时可以滚动
    QScrollArea *dataScrollArea = new QScrollArea();
    dataScrollArea->setWidgetResizable(true);
    dataScrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    dataScrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    dataScrollArea->setWidget(dataPage);

    // 添加页面到标签页
    tabWidget->addTab(dataScrollArea, "数据面板");
    tabWidget->addTab(chartPage, "图表");
    tabWidget->addTab(imagePage, "图像");
    tabWidget->addTab(autoControlPage, "自动控制");
    tabWidget->addTab(manualControlPage, "手动控制");

    // 用一个容器把顶部工具栏和 tabWidget 竖直堆叠
    QWidget *central = new QWidget(this);
    QVBoxLayout *centralLayout = new QVBoxLayout(central);
    centralLayout->setContentsMargins(0, 0, 0, 0);
    if (topToolbar) {
        centralLayout->addWidget(topToolbar);
    }
    centralLayout->addWidget(tabWidget);

    // 设置中央窗口部件
    setCentralWidget(central);

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

void MainWindow::setupTopToolbar()
{
    topToolbar = new QWidget(this);
    QHBoxLayout *layout = new QHBoxLayout(topToolbar);
    layout->setContentsMargins(8, 4, 8, 4);

    mqttStatusLabel = new QLabel(tr("MQTT: 未连接"), topToolbar);
    QPushButton *reconnectBtn = new QPushButton(tr("重新连接"), topToolbar);

    layout->addWidget(mqttStatusLabel);
    layout->addStretch();
    layout->addWidget(reconnectBtn);

    connect(reconnectBtn, &QPushButton::clicked, this, &MainWindow::reconnectMqtt);
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

void MainWindow::setupAutoControlPage()
{
    // 外层布局 + 滚动区域，避免界面过长限制窗口最小高度
    QVBoxLayout *outerLayout = new QVBoxLayout(autoControlPage);

    QScrollArea *scrollArea = new QScrollArea();
    scrollArea->setWidgetResizable(true);
    scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);

    QWidget *container = new QWidget();
    QVBoxLayout *layout = new QVBoxLayout(container);

    QLabel *hint = new QLabel(tr("通过 MQTT 向设备下发自动控制阈值，\n设备侧会在 C++ 线程中按阈值自动控制水泵/灯光/风扇/蜂鸣器。"));
    hint->setWordWrap(true);
    layout->addWidget(hint);

    autoControlEnableCheck = new QCheckBox(tr("启用自动控制"));
    layout->addWidget(autoControlEnableCheck);

    auto makeSpinRow = [&](const QString &labelText, QWidget *editor) {
        QHBoxLayout *row = new QHBoxLayout();
        QLabel *lab = new QLabel(labelText);
        // 允许多行显示，避免在窗口较窄时文字被压扁
        lab->setWordWrap(true);
        lab->setMinimumWidth(120);
        row->addWidget(lab);
        row->addWidget(editor, 1);
        layout->addLayout(row);
    };

    soilOnSpin = new QDoubleSpinBox();
    soilOnSpin->setRange(0, 100000);
    soilOnSpin->setValue(1200);
    makeSpinRow(tr("土壤干阈值 soil_on"), soilOnSpin);

    soilOffSpin = new QDoubleSpinBox();
    soilOffSpin->setRange(0, 100000);
    soilOffSpin->setValue(1600);
    makeSpinRow(tr("土壤湿阈值 soil_off"), soilOffSpin);

    lightOnSpin = new QDoubleSpinBox();
    lightOnSpin->setRange(0, 100);
    lightOnSpin->setDecimals(0);
    lightOnSpin->setValue(30);
    makeSpinRow(tr("光照暗阈值 light_on (0-100，越大越亮)"), lightOnSpin);

    lightOffSpin = new QDoubleSpinBox();
    lightOffSpin->setRange(0, 100);
    lightOffSpin->setDecimals(0);
    lightOffSpin->setValue(50);
    makeSpinRow(tr("光照亮阈值 light_off (0-100，越大越亮)"), lightOffSpin);

    tempOnSpin = new QDoubleSpinBox();
    tempOnSpin->setRange(-40, 100);
    tempOnSpin->setDecimals(1);
    tempOnSpin->setValue(30.0);
    makeSpinRow(tr("温度高阈值 temp_on (°C)"), tempOnSpin);

    tempOffSpin = new QDoubleSpinBox();
    tempOffSpin->setRange(-40, 100);
    tempOffSpin->setDecimals(1);
    tempOffSpin->setValue(27.0);
    makeSpinRow(tr("温度低阈值 temp_off (°C)"), tempOffSpin);

    co2OnSpin = new QDoubleSpinBox();
    co2OnSpin->setRange(0.0, 100000.0);
    co2OnSpin->setDecimals(1);
    co2OnSpin->setValue(1200.0);
    makeSpinRow(tr("CO2 白天高阈值 co2_on (ppm)"), co2OnSpin);

    co2OffSpin = new QDoubleSpinBox();
    co2OffSpin->setRange(0.0, 100000.0);
    co2OffSpin->setDecimals(1);
    co2OffSpin->setValue(800.0);
    makeSpinRow(tr("CO2 白天低阈值 co2_off (ppm)"), co2OffSpin);

    co2NightOnSpin = new QDoubleSpinBox();
    co2NightOnSpin->setRange(0.0, 100000.0);
    co2NightOnSpin->setDecimals(1);
    co2NightOnSpin->setValue(1200.0);
    makeSpinRow(tr("CO2 夜间高阈值 co2_night_on (ppm)"), co2NightOnSpin);

    co2NightOffSpin = new QDoubleSpinBox();
    co2NightOffSpin->setRange(0.0, 100000.0);
    co2NightOffSpin->setDecimals(1);
    co2NightOffSpin->setValue(800.0);
    makeSpinRow(tr("CO2 夜间低阈值 co2_night_off (ppm)"), co2NightOffSpin);

    // 养分/酸碱报警阈值（使用成员变量，供 publishAutoControlCommand 读取）
    phMinSpin = new QDoubleSpinBox();
    phMinSpin->setRange(0.0, 14.0);
    phMinSpin->setDecimals(2);
    phMinSpin->setValue(5.5);
    makeSpinRow(tr("pH 最小值 ph_min"), phMinSpin);

    phMaxSpin = new QDoubleSpinBox();
    phMaxSpin->setRange(0.0, 14.0);
    phMaxSpin->setDecimals(2);
    phMaxSpin->setValue(8.5);
    makeSpinRow(tr("pH 最大值 ph_max"), phMaxSpin);

    ecMinSpin = new QDoubleSpinBox();
    ecMinSpin->setRange(0.0, 100000.0);
    ecMinSpin->setDecimals(1);
    ecMinSpin->setValue(0.0);
    makeSpinRow(tr("EC 最小值 ec_min"), ecMinSpin);

    ecMaxSpin = new QDoubleSpinBox();
    ecMaxSpin->setRange(0.0, 100000.0);
    ecMaxSpin->setDecimals(1);
    ecMaxSpin->setValue(5000.0);
    makeSpinRow(tr("EC 最大值 ec_max"), ecMaxSpin);

    nMinSpin = new QDoubleSpinBox();
    nMinSpin->setRange(0.0, 100000.0);
    nMinSpin->setDecimals(1);
    nMinSpin->setValue(0.0);
    makeSpinRow(tr("氮(N) 最小值 n_min"), nMinSpin);

    nMaxSpin = new QDoubleSpinBox();
    nMaxSpin->setRange(0.0, 100000.0);
    nMaxSpin->setDecimals(1);
    nMaxSpin->setValue(3000.0);
    makeSpinRow(tr("氮(N) 最大值 n_max"), nMaxSpin);

    pMinSpin = new QDoubleSpinBox();
    pMinSpin->setRange(0.0, 100000.0);
    pMinSpin->setDecimals(1);
    pMinSpin->setValue(0.0);
    makeSpinRow(tr("磷(P) 最小值 p_min"), pMinSpin);

    pMaxSpin = new QDoubleSpinBox();
    pMaxSpin->setRange(0.0, 100000.0);
    pMaxSpin->setDecimals(1);
    pMaxSpin->setValue(3000.0);
    makeSpinRow(tr("磷(P) 最大值 p_max"), pMaxSpin);

    kMinSpin = new QDoubleSpinBox();
    kMinSpin->setRange(0.0, 100000.0);
    kMinSpin->setDecimals(1);
    kMinSpin->setValue(0.0);
    makeSpinRow(tr("钾(K) 最小值 k_min"), kMinSpin);

    kMaxSpin = new QDoubleSpinBox();
    kMaxSpin->setRange(0.0, 100000.0);
    kMaxSpin->setDecimals(1);
    kMaxSpin->setValue(3000.0);
    makeSpinRow(tr("钾(K) 最大值 k_max"), kMaxSpin);

    fanSpeedSpin = new QSpinBox();
    fanSpeedSpin->setRange(0, 100);
    fanSpeedSpin->setValue(80);
    makeSpinRow(tr("风扇速度 fan_speed (%)"), fanSpeedSpin);

    QPushButton *applyBtn = new QPushButton(tr("下发阈值到设备"));
    layout->addWidget(applyBtn);

    connect(applyBtn, &QPushButton::clicked, this, &MainWindow::publishAutoControlCommand);

    scrollArea->setWidget(container);
    outerLayout->addWidget(scrollArea);
}

void MainWindow::setupManualControlPage()
{
    // 外层布局 + 滚动区域，避免界面过长限制窗口最小高度
    QVBoxLayout *outerLayout = new QVBoxLayout(manualControlPage);

    QScrollArea *scrollArea = new QScrollArea();
    scrollArea->setWidgetResizable(true);
    scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);

    QWidget *container = new QWidget();
    QVBoxLayout *layout = new QVBoxLayout(container);

    QLabel *manualHint = new QLabel(tr("单独的执行器操作（立即控制一次，不依赖自动控制开关）"));
    manualHint->setWordWrap(true);
    layout->addWidget(manualHint);

    // 水泵
    {
        QHBoxLayout *row = new QHBoxLayout();
        QLabel *lab = new QLabel(tr("水泵"));
        lab->setMinimumWidth(120);
        QPushButton *onBtn = new QPushButton(tr("开启"));
        QPushButton *offBtn = new QPushButton(tr("关闭"));
        row->addWidget(lab);
        row->addWidget(onBtn);
        row->addWidget(offBtn);
        layout->addLayout(row);
        connect(onBtn, &QPushButton::clicked, this, &MainWindow::publishPumpOn);
        connect(offBtn, &QPushButton::clicked, this, &MainWindow::publishPumpOff);
    }

    // LED
    {
        QHBoxLayout *row = new QHBoxLayout();
        QLabel *lab = new QLabel(tr("LED"));
        lab->setMinimumWidth(120);
        QPushButton *onBtn = new QPushButton(tr("点亮"));
        QPushButton *offBtn = new QPushButton(tr("熄灭"));
        row->addWidget(lab);
        row->addWidget(onBtn);
        row->addWidget(offBtn);
        layout->addLayout(row);
        connect(onBtn, &QPushButton::clicked, this, &MainWindow::publishLedOn);
        connect(offBtn, &QPushButton::clicked, this, &MainWindow::publishLedOff);
    }

    // 风扇（使用当前风扇速度作为 direct fan 命令的速度）
    {
        QHBoxLayout *row = new QHBoxLayout();
        QLabel *lab = new QLabel(tr("风扇"));
        lab->setMinimumWidth(120);
        QPushButton *onBtn = new QPushButton(tr("启动"));
        QPushButton *offBtn = new QPushButton(tr("停止"));
        row->addWidget(lab);
        row->addWidget(onBtn);
        row->addWidget(offBtn);
        layout->addLayout(row);
        connect(onBtn, &QPushButton::clicked, this, &MainWindow::publishFanStart);
        connect(offBtn, &QPushButton::clicked, this, &MainWindow::publishFanStop);
    }

    // 蜂鸣器
    {
        QHBoxLayout *row = new QHBoxLayout();
        QLabel *lab = new QLabel(tr("蜂鸣器"));
        lab->setMinimumWidth(120);
        QPushButton *onBtn = new QPushButton(tr("打开"));
        QPushButton *offBtn = new QPushButton(tr("关闭"));
        row->addWidget(lab);
        row->addWidget(onBtn);
        row->addWidget(offBtn);
        layout->addLayout(row);
        connect(onBtn, &QPushButton::clicked, this, &MainWindow::publishBuzzerOn);
        connect(offBtn, &QPushButton::clicked, this, &MainWindow::publishBuzzerOff);
    }

    // 舵机角度
    {
        QHBoxLayout *row = new QHBoxLayout();
        QLabel *lab = new QLabel(tr("舵机角度 (0-180°)"));
        lab->setMinimumWidth(120);
        servoAngleSpin = new QSpinBox();
        servoAngleSpin->setRange(0, 180);
        servoAngleSpin->setValue(90);
        QPushButton *applyServo = new QPushButton(tr("设置角度"));
        row->addWidget(lab);
        row->addWidget(servoAngleSpin);
        row->addWidget(applyServo);
        layout->addLayout(row);
        connect(applyServo, &QPushButton::clicked, this, &MainWindow::publishServoAngle);
    }

    // 拍照
    {
        QHBoxLayout *row = new QHBoxLayout();
        QLabel *lab = new QLabel(tr("拍照"));
        lab->setMinimumWidth(120);
        QPushButton *captureBtn = new QPushButton(tr("拍照一次"));
        row->addWidget(lab);
        row->addWidget(captureBtn);
        layout->addLayout(row);
        connect(captureBtn, &QPushButton::clicked, this, &MainWindow::publishCapture);
    }

    scrollArea->setWidget(container);
    outerLayout->addWidget(scrollArea);
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
        const QString topicInput = dialog.dataTopicName().trimmed();

        // 输入框仅用于指定命名空间前缀（默认 ciallo_ohos），后续一律通过 retained announce 自动发现。
        mqttTopicPrefix = QStringLiteral("ciallo_ohos");
        if (!topicInput.isEmpty()) {
            const int slash = topicInput.indexOf('/');
            mqttTopicPrefix = (slash > 0) ? topicInput.left(slash) : topicInput;
        }

        // 发现主题固定：Qt 启动后先订阅该主题以自动发现设备（retained）。
        mqttDiscoveryFilter = QStringLiteral("%1/announce/#").arg(mqttTopicPrefix);

        // 控制/数据 topic 将在 discovery 后自动推导。
        mqttControlTopic.clear();
        mqttDataTopic.clear();
        currentDeviceId.clear();

        // 只要 broker 配置完整即可连接；data topic 可留空。
        if (!mqttBrokerAddress.isEmpty()) {
            connectMQTT();
        } else {
            QMessageBox::warning(this, "配置不完整", "MQTT服务器地址是必填的。Topic 输入框仅用于设置前缀，设备发现将自动完成。 ");
        }
    } else {
        // 用户取消了，可以退出应用或使用默认值
        QMessageBox::information(this, "MQTT连接", "未配置MQTT连接，将使用默认设置或无法接收数据。");
    }
}

void MainWindow::connectMQTT()
{
    ensureMqttDisconnected();

    mqttClient = new QMqttClient(this);
    mqttClient->setHostname(mqttBrokerAddress);
    mqttClient->setPort(mqttBrokerPort);

    // 连接信号槽
    connect(mqttClient, &QMqttClient::connected, this, &MainWindow::onMQTTConnected);
    connect(mqttClient, &QMqttClient::messageReceived, this, &MainWindow::onMQTTMessageReceived);
    connect(mqttClient, &QMqttClient::stateChanged, this, &MainWindow::handleMqttStateChanged);

    // 连接到MQTT代理
    mqttClient->connectToHost();
}

void MainWindow::ensureMqttDisconnected()
{
    if (mqttClient) {
        if (mqttClient->state() != QMqttClient::Disconnected) {
            mqttClient->disconnectFromHost();
        }
        mqttClient->deleteLater();
        mqttClient = nullptr;
    }
}

void MainWindow::handleMqttStateChanged(QMqttClient::ClientState state)
{
    if (mqttStatusLabel) {
        switch (state) {
        case QMqttClient::Disconnected:
            mqttStatusLabel->setText(tr("MQTT: 未连接"));
            break;
        case QMqttClient::Connecting:
            mqttStatusLabel->setText(tr("MQTT: 连接中..."));
            break;
        case QMqttClient::Connected:
            mqttStatusLabel->setText(tr("MQTT: 已连接"));
            break;
        }
    }

    if (state == QMqttClient::Connected) {
        mqttReconnectAttempts = 0;
        if (mqttReconnectTimer && mqttReconnectTimer->isActive()) {
            mqttReconnectTimer->stop();
        }
        return;
    }

    if (state == QMqttClient::Disconnected) {
        // 若未配置地址或主题，则不自动重连
        if (mqttBrokerAddress.isEmpty()) {
            return;
        }

        // 超过最大重试次数则停止自动重连
        const int maxAttempts = 5;
        if (mqttReconnectAttempts >= maxAttempts) {
            return;
        }

        if (!mqttReconnectTimer) {
            mqttReconnectTimer = new QTimer(this);
            mqttReconnectTimer->setSingleShot(true);
            connect(mqttReconnectTimer, &QTimer::timeout, this, [this, maxAttempts]() {
                if (mqttBrokerAddress.isEmpty()) {
                    return;
                }

                if (mqttReconnectAttempts >= maxAttempts) {
                    return;
                }

                ++mqttReconnectAttempts;
                reconnectMqtt();

                if (mqttReconnectAttempts >= maxAttempts) {
                    QMessageBox::warning(this,
                                         tr("MQTT 重连失败"),
                                         tr("已连续尝试重连 MQTT 超过 5 次，停止自动重试。"));
                }
            });
        }

        if (!mqttReconnectTimer->isActive()) {
            mqttReconnectTimer->start(5000); // 5 秒后自动尝试重连
        }
    }
}

void MainWindow::reconnectMqtt()
{
    // 手动重连重置计数器
    if (mqttReconnectTimer && mqttReconnectTimer->isActive()) {
        mqttReconnectTimer->stop();
    }
    mqttReconnectAttempts = 0;

    if (mqttBrokerAddress.isEmpty()) {
        // 若尚未配置，弹出配置对话框
        showMqttConfigDialog();
        return;
    }

    connectMQTT();
}

void MainWindow::onMQTTConnected()
{
    qDebug() << "已连接到MQTT代理";

    // 订阅 discovery（优先）：能立刻收到 retained 设备信息。
    if (mqttDiscoveryFilter.isEmpty()) {
        if (mqttTopicPrefix.isEmpty()) {
            mqttTopicPrefix = QStringLiteral("ciallo_ohos");
        }
        mqttDiscoveryFilter = QStringLiteral("%1/announce/#").arg(mqttTopicPrefix);
    }
    QMqttSubscription *discoverySub = mqttClient->subscribe(QMqttTopicFilter(mqttDiscoveryFilter), 1);
    if (!discoverySub) {
        qDebug() << "订阅发现主题失败:" << mqttDiscoveryFilter;
    } else {
        qDebug() << "成功订阅发现主题:" << mqttDiscoveryFilter;
    }
}

void MainWindow::onMQTTMessageReceived(const QByteArray &message, const QMqttTopicName &topic)
{
    // 检查主题并处理消息
    QString topicStr = topic.name();

    // discovery: <prefix>/announce/<deviceId>
    if (!mqttTopicPrefix.isEmpty() && topicStr.startsWith(mqttTopicPrefix + QStringLiteral("/announce/"))) {
        QJsonDocument doc = QJsonDocument::fromJson(message);
        if (!doc.isObject()) {
            qDebug() << "discovery JSON解析失败";
            return;
        }

        QJsonObject obj = doc.object();
        QString deviceId = obj.value("deviceId").toString();
        if (deviceId.isEmpty()) {
            return;
        }

        currentDeviceId = deviceId;
        mqttDataTopic = QStringLiteral("%1/%2/sensors").arg(mqttTopicPrefix, deviceId);
        mqttControlTopic = QStringLiteral("%1/%2/control").arg(mqttTopicPrefix, deviceId);

        // discovery 可能先于数据到达：提前显示 deviceId
        labelDeviceId->setText(QString("<b>🔌 设备ID:</b><br><span style='font-size:16px;'>%1</span>").arg(deviceId));

        if (mqttClient && mqttClient->state() == QMqttClient::Connected) {
            QMqttSubscription *dataSubscription = mqttClient->subscribe(QMqttTopicFilter(mqttDataTopic));
            if (!dataSubscription) {
                qDebug() << "订阅推导数据主题失败:" << mqttDataTopic;
            } else {
                qDebug() << "成功订阅推导数据主题:" << mqttDataTopic;
            }
        }
        return;
    }

    if (topicStr == mqttDataTopic) {
        // 处理传感器数据
        try {
            QJsonDocument doc = QJsonDocument::fromJson(message);
            if (doc.isNull() || !doc.isObject()) {
                qDebug() << "JSON解析失败";
                return;
            }

            QJsonObject obj = doc.object();

            // 提取顶层属性（UI 展示用；topic 绑定以 discovery 为准）
            QString deviceId = obj["deviceId"].toString();
            QString timestamp = obj["timestamp"].toString();
            
            // 检查sensors对象是否存在
            if (!obj.contains("sensors") || !obj["sensors"].isObject()) {
                qDebug() << "JSON格式错误: 没有找到sensors对象";
                return;
            }
            
            // 获取sensors对象
            QJsonObject sensors = obj["sensors"].toObject();
            
            // 从 sensors 对象中提取数据（环境 + 土壤/养分）
            int soilMoisture = sensors["soilMoisture"].toInt();
            int lightLevel = sensors["lightLevel"].toInt();
            double temperature = sensors["temperature"].toDouble();
            int humidity = sensors["humidity"].toInt();
            double formaldehyde = sensors["formaldehyde"].toDouble();
            double tvoc = sensors["tvoc"].toDouble();
            int co2 = sensors["co2"].toInt();

            double soilTemp = sensors["soilTemperature"].toDouble();
            double ec = sensors["ec"].toDouble();
            double ph = sensors["ph"].toDouble();
            double nVal = sensors["nitrogen"].toDouble();
            double pVal = sensors["phosphorus"].toDouble();
            double kVal = sensors["potassium"].toDouble();
            double saltVal = sensors["salt"].toDouble();
            double tdsVal = sensors["tds"].toDouble();

            int alarm = sensors.contains("alarm") ? sensors["alarm"].toInt() : 0;

            // 可选：同一条 JSON 中携带 Base64 图片
            if (obj.contains("image") && obj["image"].isString()) {
                const QByteArray imageBase64 = obj["image"].toString().toUtf8();
                const QByteArray imageData = QByteArray::fromBase64(imageBase64);

                QImage image;
                int maxWidth = imageLabel->width() - 40;
                if (image.loadFromData(imageData)) {
                    if (image.width() > maxWidth) {
                        image = image.scaledToWidth(maxWidth, Qt::SmoothTransformation);
                    }
                    imageLabel->setPixmap(QPixmap::fromImage(image));
                } else {
                    qDebug() << "图像解码失败：无法从 JSON.image 的 Base64 数据加载图像";
                }
            }

            // 更新UI
            labelDeviceId->setText(QString("<b>🔌 设备ID:</b><br><span style='font-size:16px;'>%1</span>").arg(deviceId));
            labelTimestamp->setText(QString("<b>🕕 时间戳:</b><br><span style='font-size:16px;'>%1</span>").arg(timestamp));
            labelSoilMoisture->setText(QString("<b>🌱 土壤湿度:</b><br><span style='font-size:16px;'>%1</span>").arg(soilMoisture));
            labelLightLevel->setText(QString("<b>☀️ 光照强度:</b><br><span style='font-size:16px;'>%1 %</span>").arg(lightLevel));
            labelTemperature->setText(QString("<b>🌡️ 温度:</b><br><span style='font-size:16px;'>%1 °C</span>").arg(temperature));
            labelHumidity->setText(QString("<b>💧 湿度:</b><br><span style='font-size:16px;'>%1 %</span>").arg(humidity));
            labelFormaldehyde->setText(QString("<b>🧪 甲醛:</b><br><span style='font-size:16px;'>%1 ppm</span>").arg(formaldehyde));
            labelTvoc->setText(QString("<b>🏭 TVOC:</b><br><span style='font-size:16px;'>%1 ppm</span>").arg(tvoc));
            labelCo2->setText(QString("<b>🌎 CO₂:</b><br><span style='font-size:16px;'>%1 ppm</span>").arg(co2));
            labelSoilTemp->setText(QString("<b>🌡️ 土壤温度:</b><br><span style='font-size:16px;'>%1 °C</span>").arg(soilTemp, 0, 'f', 1));
            labelEc->setText(QString("<b>⚡ EC:</b><br><span style='font-size:16px;'>%1 mS/cm</span>").arg(ec, 0, 'f', 2));
            labelPh->setText(QString("<b>📏 pH:</b><br><span style='font-size:16px;'>%1</span>").arg(ph, 0, 'f', 2));
            labelN->setText(QString("<b>🧪 氮(N):</b><br><span style='font-size:16px;'>%1 mg/kg</span>").arg(nVal, 0, 'f', 1));
            labelP->setText(QString("<b>🧪 磷(P):</b><br><span style='font-size:16px;'>%1 mg/kg</span>").arg(pVal, 0, 'f', 1));
            labelK->setText(QString("<b>🧪 钾(K):</b><br><span style='font-size:16px;'>%1 mg/kg</span>").arg(kVal, 0, 'f', 1));
            labelSalt->setText(QString("<b>🧂 盐分:</b><br><span style='font-size:16px;'>%1 mg/kg</span>").arg(saltVal, 0, 'f', 1));
            labelTds->setText(QString("<b>💧 TDS:</b><br><span style='font-size:16px;'>%1 ppm</span>").arg(tdsVal, 0, 'f', 1));

            if (alarm != 0) {
                labelPh->setText(labelPh->text() + QString("<br><span style='color:red;'>⚠ 报警: 养分/酸碱超出阈值</span>"));
            }

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
    // 只设置最小宽度，避免叠加行数导致窗口最小高度过大
    label->setMinimumWidth(140);
    return label;
}

void MainWindow::publishAutoControlCommand()
{
    QJsonObject mode;
    mode["enabled"] = autoControlEnableCheck && autoControlEnableCheck->isChecked();
    mode["soil_on"] = soilOnSpin ? soilOnSpin->value() : 1200.0;
    mode["soil_off"] = soilOffSpin ? soilOffSpin->value() : 1600.0;
    mode["light_on"] = lightOnSpin ? lightOnSpin->value() : 30.0;
    mode["light_off"] = lightOffSpin ? lightOffSpin->value() : 50.0;
    mode["temp_on"] = tempOnSpin ? tempOnSpin->value() : 30.0;
    mode["temp_off"] = tempOffSpin ? tempOffSpin->value() : 27.0;
    mode["co2_on"] = co2OnSpin ? co2OnSpin->value() : 1200.0;
    mode["co2_off"] = co2OffSpin ? co2OffSpin->value() : 800.0;
    if (co2NightOnSpin) mode["co2_night_on"] = co2NightOnSpin->value();
    if (co2NightOffSpin) mode["co2_night_off"] = co2NightOffSpin->value();
    mode["ph_min"] = phMinSpin ? phMinSpin->value() : 5.5;
    mode["ph_max"] = phMaxSpin ? phMaxSpin->value() : 8.5;
    mode["ec_min"] = ecMinSpin ? ecMinSpin->value() : 0.0;
    mode["ec_max"] = ecMaxSpin ? ecMaxSpin->value() : 5000.0;
    mode["n_min"] = nMinSpin ? nMinSpin->value() : 0.0;
    mode["n_max"] = nMaxSpin ? nMaxSpin->value() : 3000.0;
    mode["p_min"] = pMinSpin ? pMinSpin->value() : 0.0;
    mode["p_max"] = pMaxSpin ? pMaxSpin->value() : 3000.0;
    mode["k_min"] = kMinSpin ? kMinSpin->value() : 0.0;
    mode["k_max"] = kMaxSpin ? kMaxSpin->value() : 3000.0;
    mode["fan_speed"] = fanSpeedSpin ? fanSpeedSpin->value() : 80;
    publishModeObject(mode, tr("阈值控制命令已发布到 topic: %1").arg(mqttControlTopic));
}

void MainWindow::publishModeObject(const QJsonObject &mode, const QString &successMessage)
{
    if (!mqttClient || mqttClient->state() != QMqttClient::Connected) {
        QMessageBox::warning(this, tr("MQTT 未连接"), tr("请先连接到 MQTT 服务器。"));
        return;
    }

    if (mqttControlTopic.isEmpty()) {
        QMessageBox::warning(this,
                             tr("尚未发现设备"),
                             tr("尚未收到设备 announce，无法确定控制 topic。请等待设备上线或检查订阅：%1")
                                 .arg(mqttDiscoveryFilter));
        return;
    }

    QJsonObject obj;
    obj["mode"] = mode;

    QJsonDocument doc(obj);
    QByteArray payload = doc.toJson(QJsonDocument::Compact);

    auto id = mqttClient->publish(mqttControlTopic, payload, 1, false);
    if (id == -1) {
        QMessageBox::warning(this, tr("下发失败"), tr("MQTT publish 失败，请检查连接状态。"));
    } else if (!successMessage.isEmpty()) {
        QMessageBox::information(this, tr("已下发"), successMessage);
    }
}

// 下发一次性控制命令：使用 "control" 包裹，区分于阈值配置的 "mode"。
void MainWindow::publishControlObject(const QJsonObject &control, const QString &successMessage)
{
    if (!mqttClient || mqttClient->state() != QMqttClient::Connected) {
        QMessageBox::warning(this, tr("MQTT 未连接"), tr("请先连接到 MQTT 服务器。"));
        return;
    }

    if (mqttControlTopic.isEmpty()) {
        QMessageBox::warning(this,
                             tr("尚未发现设备"),
                             tr("尚未收到设备 announce，无法确定控制 topic。请等待设备上线或检查订阅：%1")
                                 .arg(mqttDiscoveryFilter));
        return;
    }

    QJsonObject obj;
    obj["control"] = control;

    QJsonDocument doc(obj);
    QByteArray payload = doc.toJson(QJsonDocument::Compact);

    auto id = mqttClient->publish(mqttControlTopic, payload, 1, false);
    if (id == -1) {
        QMessageBox::warning(this, tr("下发失败"), tr("MQTT publish 失败，请检查连接状态。"));
    } else if (!successMessage.isEmpty()) {
        QMessageBox::information(this, tr("已下发"), successMessage);
    }
}

void MainWindow::publishPumpOn()
{
    QJsonObject control;
    control["pump"] = 1;
    publishControlObject(control, tr("已发送水泵开启命令到 %1").arg(mqttControlTopic));
}

void MainWindow::publishPumpOff()
{
    QJsonObject control;
    control["pump"] = 0;
    publishControlObject(control, tr("已发送水泵关闭命令到 %1").arg(mqttControlTopic));
}

void MainWindow::publishLedOn()
{
    QJsonObject control;
    control["led"] = 1;
    publishControlObject(control, tr("已发送 LED 点亮命令到 %1").arg(mqttControlTopic));
}

void MainWindow::publishLedOff()
{
    QJsonObject control;
    control["led"] = 0;
    publishControlObject(control, tr("已发送 LED 熄灭命令到 %1").arg(mqttControlTopic));
}

void MainWindow::publishFanStart()
{
    QJsonObject control;
    int speed = fanSpeedSpin ? fanSpeedSpin->value() : 80;
    control["fan"] = speed;
    publishControlObject(control, tr("已发送风扇启动命令到 %1 (速度 %2%)").arg(mqttControlTopic).arg(speed));
}

void MainWindow::publishFanStop()
{
    QJsonObject control;
    control["fan"] = 0;
    publishControlObject(control, tr("已发送风扇停止命令到 %1").arg(mqttControlTopic));
}

void MainWindow::publishServoAngle()
{
    QJsonObject control;
    int angle = servoAngleSpin ? servoAngleSpin->value() : 90;
    control["sg90_angle"] = angle;
    publishControlObject(control, tr("已发送舵机角度 %1° 命令到 %2").arg(angle).arg(mqttControlTopic));
}

void MainWindow::publishCapture()
{
    QJsonObject control;
    control["capture"] = 1;
    publishControlObject(control, tr("已发送拍照命令到 %1").arg(mqttControlTopic));
}

void MainWindow::publishBuzzerOn()
{
    QJsonObject control;
    control["buzzer"] = 1;
    publishControlObject(control, tr("已发送蜂鸣器打开命令到 %1").arg(mqttControlTopic));
}

void MainWindow::publishBuzzerOff()
{
    QJsonObject control;
    control["buzzer"] = 0;
    publishControlObject(control, tr("已发送蜂鸣器关闭命令到 %1").arg(mqttControlTopic));
}
