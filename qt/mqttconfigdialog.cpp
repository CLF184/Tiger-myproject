#include "mqttconfigdialog.h"
#include <QMessageBox>
#include <QApplication>
#include <QScreen>
#include <QEvent>
#include <QFocusEvent>
#include <QTimer>

MqttConfigDialog::MqttConfigDialog(QWidget *parent) : QDialog(parent)
{
    setWindowTitle("MQTT 连接配置");
    setMinimumWidth(400);
    
    // 创建内容容器
    contentWidget = new QWidget(this);
    
    // 创建表单布局
    QFormLayout *formLayout = new QFormLayout(contentWidget);
    formLayout->setSpacing(15);  // 增加间距
    formLayout->setContentsMargins(15, 15, 15, 15);  // 设置更大的边距
    
    // 创建输入框
    brokerAddressEdit = new QLineEdit(this);
    brokerPortEdit = new QLineEdit(this);
    dataTopicEdit = new QLineEdit(this);
    imageTopicEdit = new QLineEdit(this);
    
    // 设置输入框大小策略
    brokerAddressEdit->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    brokerPortEdit->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    dataTopicEdit->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    imageTopicEdit->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    
    // 设置默认值
    brokerPortEdit->setText("1883");
    
    // 添加到表单布局
    formLayout->addRow("MQTT 服务器地址:", brokerAddressEdit);
    formLayout->addRow("MQTT 服务器端口:", brokerPortEdit);
    formLayout->addRow("数据主题:", dataTopicEdit);
    formLayout->addRow("图片主题:", imageTopicEdit);
    
    // 创建按钮
    QDialogButtonBox *buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    connect(buttonBox, &QDialogButtonBox::accepted, this, &MqttConfigDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &MqttConfigDialog::reject);
    
    // 创建滚动区域
    scrollArea = new QScrollArea(this);
    scrollArea->setWidgetResizable(true);
    scrollArea->setFrameShape(QFrame::NoFrame);
    scrollArea->setWidget(contentWidget);
    
    // 创建主布局
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->addWidget(scrollArea);
    mainLayout->addWidget(buttonBox);
    
    // 应用Android特定设置
    setupAndroidSpecific();
    
    // 安装事件过滤器
    brokerAddressEdit->installEventFilter(this);
    brokerPortEdit->installEventFilter(this);
    dataTopicEdit->installEventFilter(this);
    imageTopicEdit->installEventFilter(this);
    
    // 加载保存的设置
    loadSettings();
    
    // 连接 accept 信号，以便在接受时保存设置
    connect(this, &QDialog::accepted, this, &MqttConfigDialog::saveSettings);
    
    // 应用样式
    setStyleSheet(R"(
        QLineEdit {
            padding: 8px;
            border: 1px solid #ccc;
            border-radius: 4px;
            font-size: 16px;
            min-height: 30px;
        }
        QLabel {
            font-size: 16px;
            padding: 4px;
        }
        QPushButton {
            padding: 8px 16px;
            font-size: 16px;
            min-height: 40px;
        }
    )");
}

QString MqttConfigDialog::brokerAddress() const
{
    return brokerAddressEdit->text();
}

int MqttConfigDialog::brokerPort() const
{
    bool ok;
    int port = brokerPortEdit->text().toInt(&ok);
    if (!ok || port <= 0 || port > 65535) {
        return 1883; // 默认端口
    }
    return port;
}

QString MqttConfigDialog::dataTopicName() const
{
    return dataTopicEdit->text();
}

QString MqttConfigDialog::imageTopicName() const
{
    return imageTopicEdit->text();
}

void MqttConfigDialog::loadSettings()
{
    QSettings settings("YourOrganization", "MQTTMonitor");
    
    // 使用指定的默认值
    brokerAddressEdit->setText(settings.value("mqtt/brokerAddress", "broker.emqx.io").toString());
    brokerPortEdit->setText(settings.value("mqtt/brokerPort", "1883").toString());
    dataTopicEdit->setText(settings.value("mqtt/dataTopic", "ciallo_ohos/sensors").toString());
    imageTopicEdit->setText(settings.value("mqtt/imageTopic", "ciallo_ohos/image").toString());
}

void MqttConfigDialog::saveSettings()
{
    QSettings settings("YourOrganization", "MQTTMonitor");
    
    settings.setValue("mqtt/brokerAddress", brokerAddress());
    settings.setValue("mqtt/brokerPort", brokerPort());
    settings.setValue("mqtt/dataTopic", dataTopicName());
    settings.setValue("mqtt/imageTopic", imageTopicName());
}

void MqttConfigDialog::setupAndroidSpecific()
{
#ifdef Q_OS_ANDROID
    // 防止Android软键盘弹出时调整窗口大小
    setAttribute(Qt::WA_TranslucentBackground);
    
    // 获取屏幕尺寸
    QScreen *screen = QApplication::primaryScreen();
    QRect screenGeometry = screen->geometry();
    
    // 设置对话框大小为屏幕宽度的90%，高度为屏幕高度的70%
    int dialogWidth = screenGeometry.width() * 0.9;
    int dialogHeight = screenGeometry.height() * 0.7;
    
    resize(dialogWidth, dialogHeight);
    
    // 移动到屏幕中央
    move((screenGeometry.width() - width()) / 2,
         (screenGeometry.height() - height()) / 2);
#endif
}

bool MqttConfigDialog::eventFilter(QObject *watched, QEvent *event)
{
    // 当输入框获得焦点时滚动到可见区域
    if (event->type() == QEvent::FocusIn) {
        QLineEdit *lineEdit = qobject_cast<QLineEdit*>(watched);
        if (lineEdit) {
            QRect rect = lineEdit->geometry();
            QPoint topLeft = contentWidget->mapTo(this, rect.topLeft());
            QPoint bottomRight = contentWidget->mapTo(this, rect.bottomRight());
            QRect mappedRect(topLeft, bottomRight);
            
            // 确保输入框完全可见
            scrollArea->ensureWidgetVisible(lineEdit, 0, 50);
            
            // 在Android上，适当调整位置防止错位
#ifdef Q_OS_ANDROID
            QTimer::singleShot(100, [this, lineEdit]() {
                scrollArea->ensureWidgetVisible(lineEdit, 0, 50);
            });
#endif
        }
    }
    
    return QDialog::eventFilter(watched, event);
}
