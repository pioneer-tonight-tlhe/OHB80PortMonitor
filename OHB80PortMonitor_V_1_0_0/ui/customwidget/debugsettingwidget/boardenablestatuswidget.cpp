#include "boardenablestatuswidget.h"
#include "../settingwidget/settingitemwidget.h"
#include "scheduler/scheduler.h"
#include "scheduler/tasks/send_command_task.h"
#include "app/shareddata.h"
#include "app/applogger.h"
#include "loggermanager.h"

#include <QDebug>
#include <QMessageBox>
#include <QStringList>
#include <QVector>

BoardEnableStatusWidget::BoardEnableStatusWidget(QWidget *parent)
    : SettingWidget(parent)
{
    setTitle("Board Enable Status");
    initUI();
}

BoardEnableStatusWidget::~BoardEnableStatusWidget() = default;

// ============================================================
// UI
// ============================================================

void BoardEnableStatusWidget::initUI()
{
    initQrcodeItem();
    initReadItem();
}

void BoardEnableStatusWidget::initQrcodeItem()
{
    m_qrcodeItem = new SettingItemWidget(this);
    m_qrcodeItem->setTitle("Target Device");
    m_qrcodeItem->setTip("Target device QRCode (numeric, used by Read button)");

    m_qrcodeSpinBox = new QSpinBox(m_qrcodeItem);
    m_qrcodeSpinBox->setRange(0, 99999);
    m_qrcodeSpinBox->setFixedWidth(160);

    const QStringList qrcodes = SharedData::getAllQrcodes();
    if (!qrcodes.isEmpty()) {
        bool ok = false;
        const int v = qrcodes.first().toInt(&ok);
        if (ok) m_qrcodeSpinBox->setValue(v);
        else qWarning() << "[ui][BoardEnableStatusWidget] qrcode 转换 int 失败:" << qrcodes.first();
    }

    m_qrcodeItem->addWidget("qrcode_spin", m_qrcodeSpinBox);
    addItem(m_qrcodeItem);
}

void BoardEnableStatusWidget::initReadItem()
{
    m_readItem = new SettingItemWidget(this);
    m_readItem->setTitle("Board Enable Status");
    m_readItem->setTip("Read board enable/disable status (1=Disabled, 0=Normal)");

    auto *readBtn = new QPushButton("Read", m_readItem);
    m_readItem->addWidget("read_btn", readBtn);
    connect(readBtn, &QPushButton::clicked,
            this, &BoardEnableStatusWidget::onReadBtnClicked);

    auto *readAllBtn = new QPushButton("Read All", m_readItem);
    m_readItem->addWidget("read_all_btn", readAllBtn);
    connect(readAllBtn, &QPushButton::clicked,
            this, &BoardEnableStatusWidget::onReadAllBtnClicked);

    addItem(m_readItem);
}

// ============================================================
// 槽
// ============================================================

void BoardEnableStatusWidget::onReadBtnClicked()
{
    const QString qrcode = QString::number(m_qrcodeSpinBox->value());
    submitTask(QStringList{qrcode});
}

void BoardEnableStatusWidget::onReadAllBtnClicked()
{
    const QStringList qrcodes = SharedData::getAllQrcodes();
    if (qrcodes.isEmpty()) {
        QMessageBox::warning(this, "Read Failed", "No target device available");
        return;
    }
    submitTask(qrcodes);
}

// ============================================================
// 提交任务
// ============================================================

void BoardEnableStatusWidget::submitTask(const QStringList &qrcodes)
{
    const QVector<QString> qrcodeVec(qrcodes.begin(), qrcodes.end());
    auto *task = new SendCommandTask(this);

    // ReadBoardEnable 是读指令（FC 04），不需要写参数
    task->setSendToDevices(qrcodeVec,
                           QStringLiteral("ReadBoardEnable"));

    m_readItem->setStatusWaiting();

    // 收集每台设备的读取结果
    auto *results = new QStringList();

    connect(task, &SendCommandTask::dataResult,
            this, [results](const QString &qrcode, const ModbusCommand &cmd) {
                if (cmd.received && !cmd.timedOut && !cmd.checksumError) {
                    const QByteArray &regVal = cmd.response.registerValue;
                    quint16 value = 0;
                    if (regVal.size() >= 2) {
                        value = (static_cast<quint16>(static_cast<quint8>(regVal[0])) << 8)
                              |  static_cast<quint16>(static_cast<quint8>(regVal[1]));
                    }
                    const QString status = (value == 0) ? "Normal (Enabled)" : "Disabled";
                    results->append(QString("[%1] %2 (0x%3)")
                                        .arg(qrcode)
                                        .arg(status)
                                        .arg(value, 4, 16, QChar('0')));
                } else {
                    results->append(QString("[%1] Communication FAILED").arg(qrcode));
                }
            });

    connect(task, &SendCommandTask::allFinished,
            this, [this, results]
                  (bool allSuccess, int successCount,
                   int failCount, const QStringList &failedIds) {
                Q_UNUSED(failCount)
                if (allSuccess) {
                    m_readItem->setStatusOK();
                    QMessageBox::information(
                        this, "Read Succeeded",
                        QString("All %1 device(s) read OK:\n\n%2")
                            .arg(successCount)
                            .arg(results->join("\n")));
                } else {
                    m_readItem->setStatusFailed();
                    QMessageBox::warning(
                        this, "Read Result",
                        QString("%1 succeeded, %2 failed:\n\n%3\n\nFailed devices: %4")
                            .arg(successCount)
                            .arg(failedIds.size())
                            .arg(results->join("\n"))
                            .arg(failedIds.join(", ")));
                }
                delete results;
            });

    Scheduler::instance()->submitTask(task);

    qDebug() << "[ui][BoardEnableStatusWidget][submitTask] 设备数=" << qrcodes.size();
    LoggerManager::instance().log(AppLogger::SystemLoggerPath().toStdString(), Level::INFO,
        QString("[ui][BoardEnableStatusWidget][submitTask] 设备数=%1")
            .arg(qrcodes.size()).toStdString());
}
