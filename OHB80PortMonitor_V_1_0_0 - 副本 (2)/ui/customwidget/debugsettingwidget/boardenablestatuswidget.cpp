#include "boardenablestatuswidget.h"
#include "../settingwidget/settingitemwidget.h"
#include "../modaltabledialog/modaltabledialog.h"
#include "scheduler/scheduler.h"
#include "scheduler/tasks/send_command_task.h"
#include "app/shareddata.h"
#include "app/applogger.h"
#include "loggermanager.h"

#include <QDebug>
#include <QColor>
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

    // 收集每台设备的读取结果（结构化数据）
    struct DeviceResult {
        QString qrcode;
        QString status;
        QString value;
        bool success;
    };
    auto *results = new QList<DeviceResult>();

    // 保存原始设备列表，以及设备数量用于判断显示方式
    auto *targetQrcodes = new QStringList(qrcodes);
    const bool isReadAll = qrcodes.size() > 1;

    connect(task, &SendCommandTask::dataResult,
            this, [results](const QString &qrcode, const ModbusCommand &cmd) {
                DeviceResult dr;
                dr.qrcode = qrcode;
                if (cmd.received && !cmd.timedOut && !cmd.checksumError) {
                    const QByteArray &regVal = cmd.response.registerValue;
                    quint16 value = 0;
                    if (regVal.size() >= 2) {
                        value = (static_cast<quint16>(static_cast<quint8>(regVal[0])) << 8)
                              |  static_cast<quint16>(static_cast<quint8>(regVal[1]));
                    }
                    dr.value = QString("0x%1").arg(value, 4, 16, QChar('0'));
                    dr.status = (value == 0) ? "Normal (Enabled)" : "Disabled";
                    dr.success = true;
                } else {
                    dr.status = "Communication FAILED";
                    dr.value = "-";
                    dr.success = false;
                }
                results->append(dr);
            });

    connect(task, &SendCommandTask::allFinished,
            this, [this, results, targetQrcodes, isReadAll]
                  (bool allSuccess, int successCount,
                   int failCount, const QStringList &failedIds) {
                Q_UNUSED(failCount)
                if (allSuccess) {
                    m_readItem->setStatusOK();
                } else {
                    m_readItem->setStatusFailed();
                }

                // Read All 按钮（多个设备）使用 ModalTableDialog
                if (isReadAll) {
                    // 构建表格行数据：根据原始设备列表构建，确保所有设备都显示
                    QList<QStringList> tableRows;
                    for (const QString &qrcode : *targetQrcodes) {
                        // 先在结果中查找该设备
                        bool found = false;
                        for (const DeviceResult &dr : *results) {
                            if (dr.qrcode == qrcode) {
                                tableRows.append({dr.qrcode, dr.status, dr.value});
                                found = true;
                                break;
                            }
                        }
                        // 如果不在结果中，说明是失败的设备（dataResult 信号只在成功时发出）
                        if (!found) {
                            tableRows.append({qrcode, "Communication FAILED", "-"});
                        }
                    }

                    // 使用 ModalTableDialog 显示结果
                    auto *dialog = ModalTableDialog::showAsync(
                        this,
                        QString("Board Enable Status Result"),
                        QStringList{"QRCode", "Status", "Value"},
                        tableRows);

                    // 设置颜色标记
                    if (dialog) {
                        dialog->setFieldTextColor("Status", "Normal (Enabled)", QColor(0, 150, 0));
                        dialog->setFieldTextColor("Status", "Disabled", QColor(210, 0, 0));
                        dialog->setFieldTextColor("Status", "Communication FAILED", QColor(210, 0, 0));
                    }
                } else {
                    // Read 按钮（单个设备）使用 QMessageBox
                    if (!results->isEmpty()) {
                        const DeviceResult &dr = results->first();
                        if (dr.success) {
                            QMessageBox::information(
                                this, "Read Succeeded",
                                QString("Device %1: %2 (Value: %3)")
                                    .arg(dr.qrcode).arg(dr.status).arg(dr.value));
                        } else {
                            QMessageBox::warning(
                                this, "Read Failed",
                                QString("Device %1: Communication FAILED")
                                    .arg(dr.qrcode));
                        }
                    } else {
                        QMessageBox::warning(
                            this, "Read Failed",
                            QString("Device %1: Communication FAILED")
                                .arg(targetQrcodes->first()));
                    }
                }

                delete results;
                delete targetQrcodes;
            });

    Scheduler::instance()->submitTask(task);

    qDebug() << "[ui][BoardEnableStatusWidget][submitTask] 设备数=" << qrcodes.size();
    LoggerManager::getInstance()->log(AppLogger::SystemLoggerPath().toStdString(), Level::INFO,
        QString("[ui][BoardEnableStatusWidget][submitTask] 设备数=%1")
            .arg(qrcodes.size()).toStdString());
}
