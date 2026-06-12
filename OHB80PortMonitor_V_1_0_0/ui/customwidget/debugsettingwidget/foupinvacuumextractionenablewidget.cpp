/**
 * @file foupinvacuumextractionenablewidget.cpp
 * @brief FOUP IN 真空阀保持模式调试控件实现。
 * @author Simon（工号：13）
 * @date 2026-06-12
 */

#include "foupinvacuumextractionenablewidget.h"

#include "../modaltabledialog/modaltabledialog.h"
#include "../settingwidget/settingitemwidget.h"
#include "app/applogger.h"
#include "app/shareddata.h"
#include "loggermanager.h"
#include "modbustcpmastermanager/modbuscommand/modbuscommand.h"
#include "scheduler/scheduler.h"
#include "scheduler/tasks/send_command_task/send_command_task.h"

#include <QColor>
#include <QDebug>
#include <QMessageBox>
#include <QStringList>
#include <QVector>

namespace {

const QString ReadCommandId = QStringLiteral("ReadFoupInVacuumExtractionEnable");
const QString WriteCommandId = QStringLiteral("WriteFoupInVacuumExtractionEnable");

} // namespace

FoupInVacuumExtractionEnableWidget::FoupInVacuumExtractionEnableWidget(QWidget* parent)
    : SettingWidget(parent)
{
    setTitle(QStringLiteral("FOUP IN Vacuum Extraction"));
    initUI();
}

FoupInVacuumExtractionEnableWidget::~FoupInVacuumExtractionEnableWidget() = default;

void FoupInVacuumExtractionEnableWidget::initUI()
{
    initQrcodeItem();
    initControlItem();
    initBatchItem();
}

void FoupInVacuumExtractionEnableWidget::initQrcodeItem()
{
    m_qrcodeItem = new SettingItemWidget(this);
    m_qrcodeItem->setTitle(QStringLiteral("Target Device"));
    m_qrcodeItem->setTip(QStringLiteral("Target device QRCode for FOUP IN vacuum extraction read/write"));

    m_qrcodeSpinBox = new QSpinBox(m_qrcodeItem);
    m_qrcodeSpinBox->setRange(0, 99999);
    m_qrcodeSpinBox->setFixedWidth(160);

    const QStringList qrcodes = SharedData::getAllQrcodes();
    if (!qrcodes.isEmpty()) {
        bool ok = false;
        const int qrcodeValue = qrcodes.first().toInt(&ok);
        if (ok) {
            m_qrcodeSpinBox->setValue(qrcodeValue);
        } else {
            qWarning() << "[ui][FoupInVacuumExtractionEnableWidget] qrcode to int failed:"
                       << qrcodes.first();
        }
    }

    m_qrcodeItem->addWidget(QStringLiteral("qrcode_spin"), m_qrcodeSpinBox);
    addItem(m_qrcodeItem);
}

void FoupInVacuumExtractionEnableWidget::initControlItem()
{
    m_controlItem = new SettingItemWidget(this);
    m_controlItem->setTitle(QStringLiteral("Vacuum Extraction Enable"));
    m_controlItem->setTip(QStringLiteral("0=Default: close SW5 after FOUP IN 10s; 1=Keep: keep SW5 vacuum valve open while FOUP IN"));

    m_enableComboBox = new QComboBox(m_controlItem);
    m_enableComboBox->addItem(QStringLiteral("Default - close after 10s"), 0);
    m_enableComboBox->addItem(QStringLiteral("Keep - stay open"), 1);
    m_enableComboBox->setFixedWidth(220);
    m_controlItem->addWidget(QStringLiteral("enable_combo"), m_enableComboBox);

    m_readBtn = new QPushButton(QStringLiteral("Read"), m_controlItem);
    m_controlItem->addWidget(QStringLiteral("read_btn"), m_readBtn);
    connect(m_readBtn, &QPushButton::clicked,
            this, &FoupInVacuumExtractionEnableWidget::onReadBtnClicked);

    m_setBtn = new QPushButton(QStringLiteral("Set"), m_controlItem);
    m_controlItem->addWidget(QStringLiteral("set_btn"), m_setBtn);
    connect(m_setBtn, &QPushButton::clicked,
            this, &FoupInVacuumExtractionEnableWidget::onSetBtnClicked);

    addItem(m_controlItem);
}

void FoupInVacuumExtractionEnableWidget::initBatchItem()
{
    m_batchItem = new SettingItemWidget(this);
    m_batchItem->setTitle(QStringLiteral("All Devices"));
    m_batchItem->setTip(QStringLiteral("Read or set FOUP IN vacuum extraction enable for all configured devices"));

    m_readAllBtn = new QPushButton(QStringLiteral("Read All"), m_batchItem);
    m_batchItem->addWidget(QStringLiteral("read_all_btn"), m_readAllBtn);
    connect(m_readAllBtn, &QPushButton::clicked,
            this, &FoupInVacuumExtractionEnableWidget::onReadAllBtnClicked);

    m_setAllBtn = new QPushButton(QStringLiteral("Set All"), m_batchItem);
    m_batchItem->addWidget(QStringLiteral("set_all_btn"), m_setAllBtn);
    connect(m_setAllBtn, &QPushButton::clicked,
            this, &FoupInVacuumExtractionEnableWidget::onSetAllBtnClicked);

    addItem(m_batchItem);
}

void FoupInVacuumExtractionEnableWidget::onReadBtnClicked()
{
    submitReadTask(QString::number(m_qrcodeSpinBox->value()));
}

void FoupInVacuumExtractionEnableWidget::onSetBtnClicked()
{
    const QString qrcode = QString::number(m_qrcodeSpinBox->value());
    const quint16 enableValue = static_cast<quint16>(m_enableComboBox->currentData().toUInt());
    submitWriteTask(qrcode, enableValue);
}

void FoupInVacuumExtractionEnableWidget::onReadAllBtnClicked()
{
    const QStringList qrcodes = SharedData::getAllQrcodes();
    if (qrcodes.isEmpty()) {
        QMessageBox::warning(this,
                             QStringLiteral("Read Failed"),
                             QStringLiteral("No target device available"));
        return;
    }

    submitReadAllTask(qrcodes);
}

void FoupInVacuumExtractionEnableWidget::onSetAllBtnClicked()
{
    const QStringList qrcodes = SharedData::getAllQrcodes();
    if (qrcodes.isEmpty()) {
        QMessageBox::warning(this,
                             QStringLiteral("Set Failed"),
                             QStringLiteral("No target device available"));
        return;
    }

    const quint16 enableValue = static_cast<quint16>(m_enableComboBox->currentData().toUInt());
    submitWriteAllTask(qrcodes, enableValue);
}

void FoupInVacuumExtractionEnableWidget::submitReadTask(const QString& qrcode)
{
    auto* task = new SendCommandTask(this);
    task->setSendToDevices(QVector<QString>{qrcode}, ReadCommandId);

    m_controlItem->setStatusWaiting(QStringLiteral("Reading..."));
    setButtonsEnabled(false);

    auto* readValue = new quint16(0);
    auto* hasReadValue = new bool(false);

    connect(task, &SendCommandTask::dataResult,
            this, [readValue, hasReadValue](const QString& /*qrcode*/, const ModbusCommand& command) {
                *readValue = parseRegisterValue(command);
                *hasReadValue = true;
            });

    connect(task, &SendCommandTask::allFinished,
            this, [this, qrcode, readValue, hasReadValue]
                  (bool allSuccess, int successCount, int failCount, const QStringList& failedIds) {
                Q_UNUSED(successCount)
                Q_UNUSED(failCount)

                if (allSuccess && *hasReadValue) {
                    const int comboIndex = m_enableComboBox->findData(static_cast<int>(*readValue));
                    if (comboIndex >= 0) {
                        m_enableComboBox->setCurrentIndex(comboIndex);
                    }

                    m_controlItem->setStatusOK(QStringLiteral("Read OK"));
                    QMessageBox::information(
                        this,
                        QStringLiteral("Read Succeeded"),
                        QStringLiteral("Device %1: %2")
                            .arg(qrcode, valueDisplayText(*readValue)));
                } else {
                    m_controlItem->setStatusFailed(QStringLiteral("Read Failed"));
                    QMessageBox::warning(
                        this,
                        QStringLiteral("Read Failed"),
                        QStringLiteral("Failed to read FOUP IN Vacuum Extraction Enable on %1.\n%2")
                            .arg(qrcode, failedIds.join(QStringLiteral(", "))));
                }

                setButtonsEnabled(true);
                delete readValue;
                delete hasReadValue;
            });

    Scheduler::instance()->submitTask(task);

    qDebug() << "[ui][FoupInVacuumExtractionEnableWidget][submitReadTask] qrcode=" << qrcode;
    LoggerManager::getInstance()->log(AppLogger::SystemLoggerPath().toStdString(), Level::INFO,
        QString("[ui][FoupInVacuumExtractionEnableWidget][submitReadTask] qrcode=%1")
            .arg(qrcode)
            .toStdString());
}

void FoupInVacuumExtractionEnableWidget::submitWriteTask(const QString& qrcode, quint16 enableValue)
{
    auto* task = new SendCommandTask(this);
    task->setSendToDevices(QVector<QString>{qrcode},
                           WriteCommandId,
                           QVector<quint16>{enableValue});

    m_controlItem->setStatusWaiting(QStringLiteral("Setting..."));
    setButtonsEnabled(false);

    connect(task, &SendCommandTask::allFinished,
            this, [this, qrcode, enableValue]
                  (bool allSuccess, int successCount, int failCount, const QStringList& failedIds) {
                Q_UNUSED(successCount)
                Q_UNUSED(failCount)

                if (allSuccess) {
                    m_controlItem->setStatusOK(QStringLiteral("Set OK"));
                    QMessageBox::information(
                        this,
                        QStringLiteral("Set Succeeded"),
                        QStringLiteral("Device %1: %2")
                            .arg(qrcode, valueDisplayText(enableValue)));
                } else {
                    m_controlItem->setStatusFailed(QStringLiteral("Set Failed"));
                    QMessageBox::warning(
                        this,
                        QStringLiteral("Set Failed"),
                        QStringLiteral("Failed to set FOUP IN Vacuum Extraction Enable on %1.\n%2")
                            .arg(qrcode, failedIds.join(QStringLiteral(", "))));
                }

                setButtonsEnabled(true);
            });

    Scheduler::instance()->submitTask(task);

    qDebug() << "[ui][FoupInVacuumExtractionEnableWidget][submitWriteTask] qrcode=" << qrcode
             << "enableValue=" << enableValue;
    LoggerManager::getInstance()->log(AppLogger::SystemLoggerPath().toStdString(), Level::INFO,
        QString("[ui][FoupInVacuumExtractionEnableWidget][submitWriteTask] qrcode=%1 enableValue=%2")
            .arg(qrcode)
            .arg(enableValue)
            .toStdString());
}

void FoupInVacuumExtractionEnableWidget::submitReadAllTask(const QStringList& qrcodes)
{
    const QVector<QString> qrcodeVec(qrcodes.begin(), qrcodes.end());
    auto* task = new SendCommandTask(this);
    task->setSendToDevices(qrcodeVec, ReadCommandId);

    m_batchItem->setStatusWaiting(QStringLiteral("Reading all..."));
    setButtonsEnabled(false);

    struct DeviceResult {
        QString qrcode;
        quint16 value = 0;
    };
    auto* results = new QList<DeviceResult>();
    auto* targetQrcodes = new QStringList(qrcodes);

    connect(task, &SendCommandTask::dataResult,
            this, [results](const QString& qrcode, const ModbusCommand& command) {
                DeviceResult result;
                result.qrcode = qrcode;
                result.value =
                    FoupInVacuumExtractionEnableWidget::parseRegisterValue(command);
                results->append(result);
            });

    connect(task, &SendCommandTask::allFinished,
            this, [this, results, targetQrcodes]
                  (bool allSuccess, int successCount, int failCount, const QStringList& failedIds) {
                Q_UNUSED(successCount)
                Q_UNUSED(failCount)

                if (allSuccess) {
                    m_batchItem->setStatusOK(QStringLiteral("Read All OK"));
                } else {
                    m_batchItem->setStatusFailed(QStringLiteral("Read All Failed"));
                }

                QList<QStringList> tableRows;
                for (const QString& qrcode : *targetQrcodes) {
                    bool found = false;
                    for (const DeviceResult& result : *results) {
                        if (result.qrcode == qrcode) {
                            tableRows.append({
                                qrcode,
                                QStringLiteral("Success"),
                                FoupInVacuumExtractionEnableWidget::valueDisplayText(result.value)
                            });
                            found = true;
                            break;
                        }
                    }

                    if (!found) {
                        const QString reason = failedIds.contains(qrcode)
                            ? QStringLiteral("Communication FAILED")
                            : QStringLiteral("No response data");
                        tableRows.append({qrcode, reason, QStringLiteral("-")});
                    }
                }

                auto* dialog = ModalTableDialog::showAsync(
                    this,
                    QStringLiteral("FOUP IN Vacuum Extraction Read All Result"),
                    QStringList{
                        QStringLiteral("QRCode"),
                        QStringLiteral("Status"),
                        QStringLiteral("Value")
                    },
                    tableRows);

                if (dialog) {
                    dialog->setFieldTextColor(QStringLiteral("Status"),
                                              QStringLiteral("Success"),
                                              QColor(0, 150, 0));
                    dialog->setFieldTextColor(QStringLiteral("Status"),
                                              QStringLiteral("Communication FAILED"),
                                              QColor(210, 0, 0));
                    dialog->setFieldTextColor(QStringLiteral("Status"),
                                              QStringLiteral("No response data"),
                                              QColor(210, 0, 0));
                }

                setButtonsEnabled(true);
                delete results;
                delete targetQrcodes;
            });

    Scheduler::instance()->submitTask(task);

    qDebug() << "[ui][FoupInVacuumExtractionEnableWidget][submitReadAllTask] deviceCount="
             << qrcodes.size();
    LoggerManager::getInstance()->log(AppLogger::SystemLoggerPath().toStdString(), Level::INFO,
        QString("[ui][FoupInVacuumExtractionEnableWidget][submitReadAllTask] deviceCount=%1")
            .arg(qrcodes.size())
            .toStdString());
}

void FoupInVacuumExtractionEnableWidget::submitWriteAllTask(const QStringList& qrcodes,
                                                            quint16 enableValue)
{
    const QVector<QString> qrcodeVec(qrcodes.begin(), qrcodes.end());
    auto* task = new SendCommandTask(this);
    task->setSendToDevices(qrcodeVec,
                           WriteCommandId,
                           QVector<quint16>{enableValue});

    m_batchItem->setStatusWaiting(QStringLiteral("Setting all..."));
    setButtonsEnabled(false);

    auto* targetQrcodes = new QStringList(qrcodes);

    connect(task, &SendCommandTask::allFinished,
            this, [this, targetQrcodes, enableValue]
                  (bool allSuccess, int successCount, int failCount, const QStringList& failedIds) {
                Q_UNUSED(successCount)
                Q_UNUSED(failCount)

                if (allSuccess) {
                    m_batchItem->setStatusOK(QStringLiteral("Set All OK"));
                } else {
                    m_batchItem->setStatusFailed(QStringLiteral("Set All Failed"));
                }

                QList<QStringList> tableRows;
                for (const QString& qrcode : *targetQrcodes) {
                    const bool failed = failedIds.contains(qrcode)
                        || (!allSuccess && failedIds.isEmpty());
                    tableRows.append({
                        qrcode,
                        failed ? QStringLiteral("Failed") : QStringLiteral("Success"),
                        FoupInVacuumExtractionEnableWidget::valueDisplayText(enableValue)
                    });
                }

                auto* dialog = ModalTableDialog::showAsync(
                    this,
                    QStringLiteral("FOUP IN Vacuum Extraction Set All Result"),
                    QStringList{
                        QStringLiteral("QRCode"),
                        QStringLiteral("Status"),
                        QStringLiteral("Value")
                    },
                    tableRows);

                if (dialog) {
                    dialog->setFieldTextColor(QStringLiteral("Status"),
                                              QStringLiteral("Success"),
                                              QColor(0, 150, 0));
                    dialog->setFieldTextColor(QStringLiteral("Status"),
                                              QStringLiteral("Failed"),
                                              QColor(210, 0, 0));
                }

                setButtonsEnabled(true);
                delete targetQrcodes;
            });

    Scheduler::instance()->submitTask(task);

    qDebug() << "[ui][FoupInVacuumExtractionEnableWidget][submitWriteAllTask] deviceCount="
             << qrcodes.size()
             << "enableValue=" << enableValue;
    LoggerManager::getInstance()->log(AppLogger::SystemLoggerPath().toStdString(), Level::INFO,
        QString("[ui][FoupInVacuumExtractionEnableWidget][submitWriteAllTask] deviceCount=%1 enableValue=%2")
            .arg(qrcodes.size())
            .arg(enableValue)
            .toStdString());
}

void FoupInVacuumExtractionEnableWidget::setButtonsEnabled(bool enabled)
{
    if (m_readBtn) {
        m_readBtn->setEnabled(enabled);
    }
    if (m_setBtn) {
        m_setBtn->setEnabled(enabled);
    }
    if (m_readAllBtn) {
        m_readAllBtn->setEnabled(enabled);
    }
    if (m_setAllBtn) {
        m_setAllBtn->setEnabled(enabled);
    }
}

quint16 FoupInVacuumExtractionEnableWidget::parseRegisterValue(const ModbusCommand& command)
{
    const QByteArray& registerValue = command.response.registerValue;
    if (registerValue.size() < 2) {
        return 0;
    }

    return (static_cast<quint16>(static_cast<quint8>(registerValue.at(0))) << 8)
           | static_cast<quint16>(static_cast<quint8>(registerValue.at(1)));
}

QString FoupInVacuumExtractionEnableWidget::valueDisplayText(quint16 value)
{
    const QString hexText = QStringLiteral("0x%1")
        .arg(QString::number(value, 16).toUpper().rightJustified(4, QLatin1Char('0')));

    if (value == 0) {
        return QStringLiteral("Default - close SW5 after FOUP IN 10s (%1)").arg(hexText);
    }
    if (value == 1) {
        return QStringLiteral("Keep - keep SW5 open while FOUP IN (%1)").arg(hexText);
    }
    return QStringLiteral("Unknown value (%1)").arg(hexText);
}
