#include "vefcgastypesettingwidget.h"
#include "../settingwidget/settingitemwidget.h"
#include "../modaltabledialog/modaltabledialog.h"
#include "scheduler/scheduler.h"
#include "scheduler/tasks/set_vefc_gas_type_task/set_vefc_gas_type_task.h"
#include "app/shareddata.h"
#include "app/applogger.h"
#include "loggermanager.h"
#include "modbustcpmastermanager/modbustcpmastermanager.h"

#include <QDebug>
#include <QColor>
#include <QMessageBox>
#include <QVector>

VEFCGasTypeSettingWidget::VEFCGasTypeSettingWidget(QWidget *parent)
    : SettingWidget(parent)
{
    setTitle("VEFC Gas Type Configuration");
    initUI();
}

VEFCGasTypeSettingWidget::~VEFCGasTypeSettingWidget() = default;

// ============================================================
// UI
// ============================================================

void VEFCGasTypeSettingWidget::initUI()
{
    initQrcodeItem();
    initGasTypeItem();
}

void VEFCGasTypeSettingWidget::initQrcodeItem()
{
    m_qrcodeItem = new SettingItemWidget(this);
    m_qrcodeItem->setTitle("Target Device");
    m_qrcodeItem->setTip("Target device QRCode (numeric, used by Set button)");

    m_qrcodeSpinBox = new QSpinBox(m_qrcodeItem);
    m_qrcodeSpinBox->setRange(0, 99999);
    m_qrcodeSpinBox->setFixedWidth(160);

    const QStringList qrcodes = SharedData::getAllQrcodes();
    if (!qrcodes.isEmpty()) {
        bool ok = false;
        const int v = qrcodes.first().toInt(&ok);
        if (ok) m_qrcodeSpinBox->setValue(v);
        else qWarning() << "[ui][VEFCGasTypeSettingWidget] qrcode 转换 int 失败:" << qrcodes.first();
    }

    m_qrcodeItem->addWidget("qrcode_spin", m_qrcodeSpinBox);
    addItem(m_qrcodeItem);
}

void VEFCGasTypeSettingWidget::initGasTypeItem()
{
    m_gasTypeItem = new SettingItemWidget(this);
    m_gasTypeItem->setTitle("VEFC Gas Type");
    m_gasTypeItem->setTip("Set VEFC gas type (power-down retentive). 0=CDA, 1=N2, 2=Ar, 3=CO2, 4=O2");

    m_gasTypeCombo = new QComboBox(m_gasTypeItem);
    m_gasTypeCombo->addItem("CDA", static_cast<int>(SetVEFCGasTypeTask::CDA));
    m_gasTypeCombo->addItem("N2",  static_cast<int>(SetVEFCGasTypeTask::N2));
    m_gasTypeCombo->addItem("Ar",  static_cast<int>(SetVEFCGasTypeTask::Ar));
    m_gasTypeCombo->addItem("CO2", static_cast<int>(SetVEFCGasTypeTask::CO2));
    m_gasTypeCombo->addItem("O2",  static_cast<int>(SetVEFCGasTypeTask::O2));
    m_gasTypeCombo->setFixedWidth(120);
    m_gasTypeItem->addWidget("gas_type_combo", m_gasTypeCombo);

    auto *setBtn = new QPushButton("Set", m_gasTypeItem);
    m_gasTypeItem->addWidget("gas_type_set_btn", setBtn);
    connect(setBtn, &QPushButton::clicked,
            this, &VEFCGasTypeSettingWidget::onSetBtnClicked);

    auto *setAllBtn = new QPushButton("Set All", m_gasTypeItem);
    m_gasTypeItem->addWidget("gas_type_set_all_btn", setAllBtn);
    connect(setAllBtn, &QPushButton::clicked,
            this, &VEFCGasTypeSettingWidget::onSetAllBtnClicked);

    addItem(m_gasTypeItem);
}

// ============================================================
// 槽
// ============================================================

void VEFCGasTypeSettingWidget::onSetBtnClicked()
{
    const QString qrcode = QString::number(m_qrcodeSpinBox->value());
    submitTask(QStringList{qrcode}, m_gasTypeCombo->currentData().toInt());
}

void VEFCGasTypeSettingWidget::onSetAllBtnClicked()
{
    const QStringList qrcodes = ModbusTcpMasterManager::instance().masterIds();
    if (qrcodes.isEmpty()) {
        QMessageBox::warning(this, "Set Failed", "No target device available");
        return;
    }
    submitTask(qrcodes, m_gasTypeCombo->currentData().toInt());
}

// ============================================================
// 提交任务
// ============================================================

void VEFCGasTypeSettingWidget::submitTask(const QStringList &qrcodes, int gasType)
{
    const QVector<QString> qrcodeVec(qrcodes.begin(), qrcodes.end());
    auto *task = new SetVEFCGasTypeTask(qrcodeVec, gasType);

    const QString gasName = m_gasTypeCombo->currentText();

    m_gasTypeItem->setStatusWaiting();

    // 保存设备列表用于后续构建表格，以及设备数量用于判断显示方式
    auto *targetQrcodes = new QStringList(qrcodes);
    const bool isSetAll = qrcodes.size() > 1;

    connect(task, &SetVEFCGasTypeTask::deviceRetrying, this,
            [this](const QString &qrCode, int retryCount, int maxRetry) {
                m_gasTypeItem->setStatusWaiting(QString("Retrying %1 (%2/%3)")
                    .arg(qrCode)
                    .arg(retryCount)
                    .arg(maxRetry));
            },
            Qt::QueuedConnection);

    connect(task, &SetVEFCGasTypeTask::allFinished,
            this, [this, targetQrcodes, isSetAll, gasName]
                  (bool allSuccess, int successCount,
                   QStringList failedQrCodes, int gasFinal) {
                if (allSuccess) {
                    m_gasTypeItem->setStatusOK();
                } else {
                    m_gasTypeItem->setStatusFailed();
                }

                // Set All 按钮（多个设备）使用 ModalTableDialog
                if (isSetAll) {
                    // 构建表格行数据：所有设备都显示设置的值
                    QList<QStringList> tableRows;
                    for (const QString &qrcode : *targetQrcodes) {
                        const bool success = !failedQrCodes.contains(qrcode);
                        const QString status = success ? "Success" : "Failed";
                        const QString gasHex = QString("0x%1")
                            .arg(QString::number(gasFinal, 16).toUpper().rightJustified(4, '0'));
                        tableRows.append({qrcode, status, gasName, gasHex});
                    }

                    // 使用 ModalTableDialog 显示结果
                    auto *dialog = ModalTableDialog::showAsync(
                        this,
                        QString("VEFC Gas Type Set Result"),
                        QStringList{"QRCode", "Status", "Gas Type", "Value"},
                        tableRows);

                    // 设置颜色标记
                    if (dialog) {
                        dialog->setFieldTextColor("Status", "Success", QColor(0, 150, 0));
                        dialog->setFieldTextColor("Status", "Failed", QColor(210, 0, 0));
                    }
                } else {
                    // Set 按钮（单个设备）使用 QMessageBox
                    const QString gasHex = QString("0x%1")
                        .arg(QString::number(gasFinal, 16).toUpper().rightJustified(4, '0'));
                    if (allSuccess) {
                        QMessageBox::information(
                            this, "Set Succeeded",
                            QString("Successfully set VEFC Gas Type=[%1 (%2)]")
                                .arg(gasName).arg(gasHex));
                    } else {
                        QMessageBox::warning(
                            this, "Set Failed",
                            QString("Failed to set VEFC Gas Type=[%1 (%2)]")
                                .arg(gasName).arg(gasHex));
                    }
                }

                delete targetQrcodes;
            });

    Scheduler::instance()->submitTask(task);

    qDebug() << "[ui][VEFCGasTypeSettingWidget][submitTask]：提交任务 设备数="
             << qrcodes.size() << "gasType=" << gasType << "(" << gasName << ")";
    LoggerManager::getInstance()->log(AppLogger::SystemLoggerPath().toStdString(), Level::INFO,
        QString("[ui][VEFCGasTypeSettingWidget][submitTask]：提交任务 设备数=%1 gasType=%2 (%3)")
            .arg(qrcodes.size()).arg(gasType).arg(gasName).toStdString());
}
