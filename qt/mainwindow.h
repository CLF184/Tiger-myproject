#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QMqttClient>
#include <QLabel>
#include <QTabWidget>
#include <QDateTime>
#include <QQueue>
#include <QJsonDocument>
#include <QJsonObject>
#include <QImageReader>
#include <QVBoxLayout>
#include <QScrollArea>

// 直接包含Qt Charts类，不使用命名空间
#include <QChartView>
#include <QLineSeries>
#include <QChart>
#include <QDateTimeAxis>
#include <QValueAxis>
#include <QCheckBox>
#include <QDoubleSpinBox>
#include <QSpinBox>

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();
    
private slots:
    void onMQTTConnected();
    void onMQTTMessageReceived(const QByteArray &message, const QMqttTopicName &topic);
    void showMqttConfigDialog(); // 新增方法
    
private:
    void setupUI();
    void connectMQTT();
    QLabel* createSensorCard(const QString &title, const QString &value);
    void setupChartPage();
    void setupImagePage();
    void setupTopToolbar();
    void updateCharts(const QDateTime &timestamp, double temperature, int humidity, int soilMoisture,
                     int lightLevel, double /*formaldehyde*/, double /*tvoc*/, int /*co2*/);

private:
    Ui::MainWindow *ui;
    QTabWidget *tabWidget;
    QWidget *dataPage;
    QWidget *chartPage;
    QWidget *imagePage;
    QWidget *autoControlPage;
    QWidget *manualControlPage;
    
    QLabel *labelDeviceId;
    QLabel *labelTimestamp;
    QLabel *labelSoilMoisture;
    QLabel *labelLightLevel;
    QLabel *labelTemperature;
    QLabel *labelHumidity;
    QLabel *labelFormaldehyde;
    QLabel *labelTvoc;
    QLabel *labelCo2;
    QLabel *labelSoilTemp;
    QLabel *labelEc;
    QLabel *labelPh;
    QLabel *labelN;
    QLabel *labelP;
    QLabel *labelK;
    QLabel *labelSalt;
    QLabel *labelTds;
    QLabel *imageLabel;
    QWidget *topToolbar = nullptr;
    QLabel *mqttStatusLabel = nullptr;
    
    QMqttClient *mqttClient;
    
    // 图表相关成员，不使用命名空间限定符
    QChart *temperatureChart;
    QChart *humidityChart;
    QChart *soilMoistureChart;
    QChart *lightLevelChart;
    QChart *co2Chart;         // 新增 CO2 图表
    QChart *tvocChart;        // 新增 TVOC 图表
    QChart *formaldehydeChart; // 新增甲醛图表
    
    QLineSeries *temperatureSeries;
    QLineSeries *humiditySeries;
    QLineSeries *soilMoistureSeries;
    QLineSeries *lightLevelSeries;
    QLineSeries *co2Series;         // 新增 CO2 数据序列
    QLineSeries *tvocSeries;        // 新增 TVOC 数据序列
    QLineSeries *formaldehydeSeries; // 新增甲醛数据序列
    
    QChartView *temperatureChartView;
    QChartView *humidityChartView;
    QChartView *soilMoistureChartView;
    QChartView *lightLevelChartView;
    QChartView *co2ChartView;         // 新增 CO2 图表视图
    QChartView *tvocChartView;        // 新增 TVOC 图表视图
    QChartView *formaldehydeChartView; // 新增甲醛图表视图
    
    QQueue<QDateTime> timeQueue;
    const int MAX_DATA_POINTS = 50; // 最大数据点数

    // 新增成员变量
    QString mqttBrokerAddress;
    int mqttBrokerPort;
    QString mqttDataTopic;
    QString mqttControlTopic;   // 自动控制命令下发 topic
    QString currentDeviceId;    // 最近一次收到的数据中的 deviceId

    // 自动控制 UI 控件
    QCheckBox *autoControlEnableCheck = nullptr;
    QDoubleSpinBox *soilOnSpin = nullptr;
    QDoubleSpinBox *soilOffSpin = nullptr;
    QDoubleSpinBox *lightOnSpin = nullptr;
    QDoubleSpinBox *lightOffSpin = nullptr;
    QDoubleSpinBox *tempOnSpin = nullptr;
    QDoubleSpinBox *tempOffSpin = nullptr;
    QDoubleSpinBox *co2OnSpin = nullptr;     // 白天 CO2 高阈值
    QDoubleSpinBox *co2OffSpin = nullptr;    // 白天 CO2 低阈值
    QDoubleSpinBox *co2NightOnSpin = nullptr;  // 夜间 CO2 高阈值
    QDoubleSpinBox *co2NightOffSpin = nullptr; // 夜间 CO2 低阈值
    QSpinBox *fanSpeedSpin = nullptr;
    QSpinBox *servoAngleSpin = nullptr;

    // 养分/酸碱报警阈值控件
    QDoubleSpinBox *phMinSpin = nullptr;
    QDoubleSpinBox *phMaxSpin = nullptr;
    QDoubleSpinBox *ecMinSpin = nullptr;
    QDoubleSpinBox *ecMaxSpin = nullptr;
    QDoubleSpinBox *nMinSpin = nullptr;
    QDoubleSpinBox *nMaxSpin = nullptr;
    QDoubleSpinBox *pMinSpin = nullptr;
    QDoubleSpinBox *pMaxSpin = nullptr;
    QDoubleSpinBox *kMinSpin = nullptr;
    QDoubleSpinBox *kMaxSpin = nullptr;

    void setupAutoControlPage();
    void setupManualControlPage();
    void publishAutoControlCommand();
    void publishModeObject(const QJsonObject &mode, const QString &successMessage);
    void publishControlObject(const QJsonObject &control, const QString &successMessage);

    // 手动执行器控制
    void publishPumpOn();
    void publishPumpOff();
    void publishLedOn();
    void publishLedOff();
    void publishFanStart();
    void publishFanStop();
    void publishBuzzerOn();
    void publishBuzzerOff();
    void publishServoAngle();
    void publishCapture();

    // MQTT 辅助
    void ensureMqttDisconnected();
    void reconnectMqtt();
};
#endif // MAINWINDOW_H
