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
    void updateCharts(const QDateTime &timestamp, double temperature, int humidity, int soilMoisture,
                     int lightLevel, double /*formaldehyde*/, double /*tvoc*/, int /*co2*/);

private:
    Ui::MainWindow *ui;
    QTabWidget *tabWidget;
    QWidget *dataPage;
    QWidget *chartPage;
    QWidget *imagePage;
    
    QLabel *labelDeviceId;
    QLabel *labelTimestamp;
    QLabel *labelSoilMoisture;
    QLabel *labelLightLevel;
    QLabel *labelTemperature;
    QLabel *labelHumidity;
    QLabel *labelFormaldehyde;
    QLabel *labelTvoc;
    QLabel *labelCo2;
    QLabel *imageLabel;
    
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
    QString mqttImageTopic;
};
#endif // MAINWINDOW_H
