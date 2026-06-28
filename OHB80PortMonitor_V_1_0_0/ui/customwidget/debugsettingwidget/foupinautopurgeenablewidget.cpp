/**
 * @file foupinautopurgeenablewidget.cpp
 * @brief FOUPIN自动充气使能调试控件实现。
 * @author Simon（工号：13）
 * @date 2026-06-25
 */

#include "foupinautopurgeenablewidget.h"

#include "../modaltabledialog/modaltabledialog.h"
#include "../settingwidget/settingitemwidget.h"
#include "app/applogger.h"
#include "app/shareddata.h"
#include "loggermanager.h"
#include "ohbdeviceconfig.h"
#include "scheduler/scheduler.h"
#include "scheduler/tasks/send_command_task/send_command_task.h"

#include <QColor>
#include <QMessageBox>
#include <QtGlobal>
#include <QVector>

namespace {

const QString WriteCommandId = QStringLiteral("WriteFoupInAutoPurgeEnable");

} // namespace

FoupInAutoPurgeEnableWidget::FoupInAutoPurgeEnableWidget(QWidget* parent)
    : SettingWidget(parent)
{
    setTitle(QStringLiteral("FOUPIN自动充气使能"));
    initUI();
}

FoupInAutoPurgeEnableWidget::~FoupInAutoPurgeEnableWidget() = default;

void FoupInAutoPurgeEnableWidget::setInitialConfigValue(int enableValue)
{
    if (!m_enableComboBox) {
        return;
    }

    const int comboIndex = m_enableComboBox->findData(qBound(0, enableValue, 1));
    if (comboIndex >= 0) {
        m_enableComboBox->setCurrentIndex(comboIndex);
    }
}

void FoupInAutoPurgeEnableWidget::initUI()
{
    initTargetItem();
    initControlItem();
}

void FoupInAutoPurgeEnableWidget::initTargetItem()
{
    m_targetItem = new SettingItemWidget(this);
    m_targetItem->setTitle(QStringLiteral("目标设备"));
    m_targetItem->setTip(QStringLiteral("选择需要设置 FOUPIN 自动充气使能的设备二维码"));

    m_qrcodeSpinBox = new QSpinBox(m_targetItem);
    m_qrcodeSpinBox->setRange(0, 99999);
    m_qrcodeSpinBox->setFixedWidth(160);

    const QStringList qrcodes = SharedData::getAllQrcodes();
    if (!qrcodes.isEmpty()) {
        bool ok = false;
        const int qrcodeValue = qrcodes.first().toInt(&ok);
        if (ok) {
            m_qrcodeSpinBox->setValue(qrcodeValue);
        }
    }

    m_targetItem->addWidget(QStringLiteral("qrcode_spin"), m_qrcodeSpinBox);
    addItem(m_targetItem);
}

void FoupInAutoPurgeEnableWidget::initControlItem()
{
    m_controlItem = new SettingItemWidget(this);
    m_controlItem->setTitle(QStringLiteral("FOUPIN自动充气使能"));
    m_controlItem->setTip(QStringLiteral("0=FOUP到位后不执行充气及相关功能；1=FOUP到位后正常充气并开启30min湿度检测"));

    m_enableComboBox = new QComboBox(m_controlItem);
    m_enableComboBox->addItem(QStringLiteral("0 - 不执行自动充气"), 0);
    m_enableComboBox->addItem(QStringLiteral("1 - 正常充气并开启30min湿度检测"), 1);
    m_enableComboBox->setFixedWidth(260);
    m_controlItem->addWidget(QStringLiteral("enable_combo"), m_enableComboBox);

    m_setSingleBtn = new QPushButton(QStringLiteral("设置单个设备"), m_controlItem);
    m_controlItem->addWidget(QStringLiteral("set_single_btn"), m_setSingleBtn);
    connect(m_setSingleBtn, &QPushButton::clicked,
            this, &FoupInAutoPurgeEnableWidget::onSetSingleBtnClicked);

    m_setAllBtn = new QPushButton(QStringLiteral("设置所有设备"), m_controlItem);
    m_controlItem->addWidget(QStringLiteral("set_all_btn"), m_setAllBtn);
    connect(m_setAllBtn, &QPushButton::clicked,
            this, &FoupInAutoPurgeEnableWidget::onSetAllBtnClicked);

    addItem(m_controlItem);
}

void FoupInAutoPurgeEnableWidget::onSetSingleBtnClicked()
{
    submitWriteTask(QString::number(m_qrcodeSpinBox->value()), currentEnableValue());
}

void FoupInAutoPurgeEnableWidget::onSetAllBtnClicked()
{
    const QStringList qrcodes = SharedData::getAllQrcodes();
    if (qrcodes.isEmpty()) {
        QMessageBox::warning(this,
                             QStringLiteral("设置失败"),
                             QStringLiteral("没有可用的目标设备"));
        return;
    }

    submitWriteAllTask(qrcodes, currentEnableValue());
}

void FoupInAutoPurgeEnableWidget::submitWriteTask(const QString& qrcode, quint16 enableValue)
{
    auto* task = new SendCommandTask(this);
    task->setSendToDevices(QVector<QString>{qrcode},
                           WriteCommandId,
                           QVector<quint16>{enableValue});

    m_controlItem->setStatusWaiting(QStringLiteral("设置中..."));
    setButtonsEnabled(false);

    connect(task, &SendCommandTask::allFinished,
            this, [this, qrcode, enableValue]
                  (bool allSuccess, int successCount, int failCount, const QStringList& failedIds) {
                Q_UNUSED(successCount)
                Q_UNUSED(failCount)

                if (allSuccess) {
                    OHBDeviceConfig::getInstance().setFoupInAutoPurgeEnableByQRCode(
                        qrcode,
                        static_cast<int>(enableValue));
                    m_controlItem->setStatusOK(QStringLiteral("设置成功"));
                    QMessageBox::information(
                        this,
                        QStringLiteral("设置成功"),
                        QStringLiteral("设备 %1：%2")
                            .arg(qrcode, valueDisplayText(enableValue)));
                } else {
                    m_controlItem->setStatusFailed(QStringLiteral("设置失败"));
                    QMessageBox::warning(
                        this,
                        QStringLiteral("设置失败"),
                        QStringLiteral("设备 %1 设置 FOUPIN 自动充气使能失败。\n%2")
                            .arg(qrcode, failedIds.join(QStringLiteral(", "))));
                }

                setButtonsEnabled(true);
            });

    Scheduler::instance()->submitTask(task);

    LoggerManager::getInstance()->log(AppLogger::SystemLoggerPath().toStdString(), Level::INFO,
        QString("[ui][FoupInAutoPurgeEnableWidget][submitWriteTask] qrcode=%1 enableValue=%2")
            .arg(qrcode)
            .arg(enableValue)
            .toStdString());
}

void FoupInAutoPurgeEnableWidget::submitWriteAllTask(const QStringList& qrcodes,
                                                     quint16 enableValue)
{
    const QVector<QString> qrcodeVec(qrcodes.begin(), qrcodes.end());
    auto* task = new SendCommandTask(this);
    task->setSendToDevices(qrcodeVec,
                           WriteCommandId,
                           QVector<quint16>{enableValue});

    m_controlItem->setStatusWaiting(QStringLiteral("全部设置中..."));
    setButtonsEnabled(false);

    auto* targetQrcodes = new QStringList(qrcodes);

    connect(task, &SendCommandTask::allFinished,
            this, [this, targetQrcodes, enableValue]
                  (bool allSuccess, int successCount, int failCount, const QStringList& failedIds) {
                Q_UNUSED(successCount)
                Q_UNUSED(failCount)

                if (allSuccess) {
                    m_controlItem->setStatusOK(QStringLiteral("全部设置成功"));
                } else {
                    m_controlItem->setStatusFailed(QStringLiteral("部分或全部设置失败"));
                }

                QList<QStringList> tableRows;
                for (const QString& qrcode : *targetQrcodes) {
                    const bool failed = failedIds.contains(qrcode)
                        || (!allSuccess && failedIds.isEmpty());
                    if (!failed) {
                        OHBDeviceConfig::getInstance().setFoupInAutoPurgeEnableByQRCode(
                            qrcode,
                            static_cast<int>(enableValue));
                    }
                    tableRows.append({
                        qrcode,
                        failed ? QStringLiteral("失败") : QStringLiteral("成功"),
                        FoupInAutoPurgeEnableWidget::valueDisplayText(enableValue)
                    });
                }

                auto* dialog = ModalTableDialog::showAsync(
                    this,
                    QStringLiteral("FOUPIN自动充气使能设置结果"),
                    QStringList{
                        QStringLiteral("二维码"),
                        QStringLiteral("状态"),
                        QStringLiteral("设置值")
                    },
                    tableRows);

                if (dialog) {
                    dialog->setFieldTextColor(QStringLiteral("状态"),
                                              QStringLiteral("成功"),
                                              QColor(0, 150, 0));
                    dialog->setFieldTextColor(QStringLiteral("状态"),
                                              QStringLiteral("失败"),
                                              QColor(210, 0, 0));
                }

                setButtonsEnabled(true);
                delete targetQrcodes;
            });

    Scheduler::instance()->submitTask(task);

    LoggerManager::getInstance()->log(AppLogger::SystemLoggerPath().toStdString(), Level::INFO,
        QString("[ui][FoupInAutoPurgeEnableWidget][submitWriteAllTask] deviceCount=%1 enableValue=%2")
            .arg(qrcodes.size())
            .arg(enableValue)
            .toStdString());
}

void FoupInAutoPurgeEnableWidget::setButtonsEnabled(bool enabled)
{
    if (m_setSingleBtn) {
        m_setSingleBtn->setEnabled(enabled);
    }
    if (m_setAllBtn) {
        m_setAllBtn->setEnabled(enabled);
    }
}

quint16 FoupInAutoPurgeEnableWidget::currentEnableValue() const
{
    return static_cast<quint16>(m_enableComboBox->currentData().toUInt());
}

QString FoupInAutoPurgeEnableWidget::valueDisplayText(quint16 value)
{
    const QString hexText = QStringLiteral("0x%1")
        .arg(QString::number(value, 16).toUpper().rightJustified(4, QLatin1Char('0')));

    if (value == 0) {
        return QStringLiteral("0 - FOUP到位后不执行充气及相关功能 (%1)").arg(hexText);
    }
    if (value == 1) {
        return QStringLiteral("1 - FOUP到位后正常充气并开启30min湿度检测 (%1)").arg(hexText);
    }
    return QStringLiteral("未知值 (%1)").arg(hexText);
}
