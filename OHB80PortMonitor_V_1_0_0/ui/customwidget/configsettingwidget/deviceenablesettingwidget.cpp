#include "deviceenablesettingwidget.h"
#include "../settingwidget/settingitemwidget.h"
#include "app/shareddata.h"
#include "app/ohbdeviceconfig.h"
#include "classes/foupofohbinfo.h"
#include "scheduler/scheduler.h"
#include "scheduler/tasks/send_command_task.h"
#include "app/applogger.h"
#include "loggermanager.h"

#include <QDebug>
#include <QMessageBox>

DeviceEnableSettingWidget::DeviceEnableSettingWidget(QWidget *parent)
    : SettingWidget(parent)
{
    setTitle("Device Enable Configuration");
    initUI();
}

DeviceEnableSettingWidget::~DeviceEnableSettingWidget() = default;

// ============================================================
// UI 初始化
// ============================================================

void DeviceEnableSettingWidget::initUI()
{
    initQrcodeItem();
    initStatusItem();
}

void DeviceEnableSettingWidget::initQrcodeItem()
{
    m_qrcodeItem = new SettingItemWidget(this);
    m_qrcodeItem->setTitle("Target Device");
    m_qrcodeItem->setTip("Target device QRCode (numeric) for enable/disable setting");

    m_qrcodeSpinBox = new QSpinBox(m_qrcodeItem);
    m_qrcodeSpinBox->setRange(0, 99999);
    m_qrcodeSpinBox->setFixedWidth(160);

    // 初始值：SharedData::getAllQrcodes() 第一个
    const QStringList qrcodes = SharedData::getAllQrcodes();
    if (!qrcodes.isEmpty()) {
        bool ok = false;
        const int v = qrcodes.first().toInt(&ok);
        if (ok) {
            m_qrcodeSpinBox->setValue(v);
        } else {
            qWarning() << "[ui][DeviceEnableSettingWidget] qrcode 转换 int 失败:" << qrcodes.first();
        }
    }

    m_qrcodeItem->addWidget("qrcode_spin", m_qrcodeSpinBox);
    addItem(m_qrcodeItem);
}

void DeviceEnableSettingWidget::initStatusItem()
{
    m_statusItem = new SettingItemWidget(this);
    m_statusItem->setTitle("Device Status");
    m_statusItem->setTip("Set device enable/disable status");

    m_statusComboBox = new QComboBox(m_statusItem);
    m_statusComboBox->addItem("Enable", true);
    m_statusComboBox->addItem("Disable", false);
    m_statusComboBox->setFixedWidth(120);
    m_statusItem->addWidget("status_combo", m_statusComboBox);

    auto *setBtn = new QPushButton("Set", m_statusItem);
    m_statusItem->addWidget("set_btn", setBtn);
    connect(setBtn, &QPushButton::clicked,
            this, &DeviceEnableSettingWidget::onSetClicked);

    addItem(m_statusItem);
}

// ============================================================
// 槽函数
// ============================================================

void DeviceEnableSettingWidget::onSetClicked()
{
    const QString qrcode = QString::number(m_qrcodeSpinBox->value());
    const bool enable = m_statusComboBox->currentData().toBool();

    m_statusItem->setStatusWaiting();

    // 先下发 Modbus 指令，成功后再持久化到配置文件
    submitBoardEnableCommand(qrcode, enable);
}

// ============================================================
// 下发 SetBoardEnable Modbus 指令
// ============================================================

void DeviceEnableSettingWidget::submitBoardEnableCommand(const QString &qrcode, bool enable)
{
    auto *task = new SendCommandTask(this);

    // SetBoardEnable: 寄存器值 1=禁用, 0=正常
    // UI "Enable" 对应正常(0), "Disable" 对应禁用(1)
    const quint16 registerValue = enable == true ? 0x0000 : 0x0001;
    task->setSendToDevices(QVector<QString>{qrcode},
                           QStringLiteral("SetBoardEnable"),
                           QVector<quint16>{registerValue});

    connect(task, &SendCommandTask::allFinished,
            this, [this, qrcode, enable]
                  (bool allSuccess, int successCount,
                   int /*failCount*/, const QStringList &failedIds) {
                Q_UNUSED(successCount)
                if (allSuccess) {
                    // 指令成功后才持久化到配置文件并更新内存
                    bool persistOk = OHBDeviceConfig::getInstance().setDeviceEnable(qrcode, enable);
                    FoupOfOHBInfo* foup = SharedData::getFoupByQRCode(qrcode);
                    if (foup) {
                        foup->setEnable(enable);
                    }

                    if (persistOk) {
                        m_statusItem->setStatusOK();
                        QMessageBox::information(
                            this,
                            "Set Succeeded",
                            QString("Successfully set device [%1] to [%2]")
                                .arg(qrcode)
                                .arg(enable ? "Enable" : "Disable"));
                    } else {
                        m_statusItem->setStatusFailed();
                        QMessageBox::warning(
                            this,
                            "Set Failed",
                            QString("Command succeeded but failed to persist config for device [%1]")
                                .arg(qrcode));
                    }
                } else {
                    m_statusItem->setStatusFailed();
                    QMessageBox::warning(
                        this,
                        "Set Failed",
                        QString("Failed to send SetBoardEnable to device [%1]:\n%2")
                            .arg(qrcode)
                            .arg(failedIds.join(", ")));
                }
            });

    Scheduler::instance()->submitTask(task);

    qDebug() << "[ui][DeviceEnableSettingWidget][submitBoardEnableCommand] qrcode=" << qrcode
             << "enable=" << enable;
    LoggerManager::instance().log(AppLogger::SystemLoggerPath().toStdString(), Level::INFO,
        QString("[ui][DeviceEnableSettingWidget] submitBoardEnableCommand qrcode=%1 enable=%2")
            .arg(qrcode).arg(enable).toStdString());
}
