#ifndef MQTTCONFIGDIALOG_H
#define MQTTCONFIGDIALOG_H

#include <QDialog>
#include <QLineEdit>
#include <QFormLayout>
#include <QPushButton>
#include <QVBoxLayout>
#include <QDialogButtonBox>
#include <QSettings>
#include <QScrollArea>  // 添加滚动区域支持

class MqttConfigDialog : public QDialog
{
    Q_OBJECT

public:
    explicit MqttConfigDialog(QWidget *parent = nullptr);

    QString brokerAddress() const;
    int brokerPort() const;
    QString dataTopicName() const;
    QString imageTopicName() const;

private:
    QLineEdit *brokerAddressEdit;
    QLineEdit *brokerPortEdit;
    QLineEdit *dataTopicEdit;
    QLineEdit *imageTopicEdit;
    QScrollArea *scrollArea;  // 添加滚动区域
    QWidget *contentWidget;   // 内容容器
    
    void loadSettings();
    void saveSettings();
    void setupAndroidSpecific();  // 添加Android特定设置
    bool eventFilter(QObject *watched, QEvent *event) override;  // 添加事件过滤器
};

#endif // MQTTCONFIGDIALOG_H
