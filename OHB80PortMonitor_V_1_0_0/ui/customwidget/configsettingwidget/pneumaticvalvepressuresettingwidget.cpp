#include "pneumaticvalvepressuresettingwidget.h"
#include "../settingwidget/settingitemwidget.h"
#include "modaltabledialog.h"
#include "ohbdeviceconfig.h"
#include "scheduler/scheduler.h"
#include "tasks/set_pneumatic_valve_pressure_task.h"
#include "app/shareddata.h"
#include "app/applogger.h"
#include "loggermanager.h"

#include <QDebug>
#include <QMessageBox>
#include <QColor>

namespace {
QList<QStringList> buildDeviceResultRows(const QStringList &targetQrCodes,
                                         const QStringList &failedQrCodes,
                                         bool allSuccess,
                                         double pressureBar)
{
    QStringList qrcodes = targetQrCodes;
    for (const QString &qrCode : failedQrCodes) {
        if (!qrcodes.contains(qrCode)) {
            qrcodes.append(qrCode);
        }
    }

    // 格式化设置值（带单位）
    QString settingValueStr = QString("Set Pressure %1 bar").arg(pressureBar);

    QList<QStringList> rows;
    for (const QString &qrCode : qrcodes) {
        const bool failed = !allSuccess
            && (failedQrCodes.isEmpty() || failedQrCodes.contains(qrCode));
        rows.append({
            qrCode,
            settingValueStr,
            failed ? QStringLiteral("Fail") : QStringLiteral("Success")
        });
    }
    return rows;
}

void applyResultColors(ModalTableDialog *dialog)
{
    if (!dialog) return;
    dialog->setFieldTextColor(QStringLiteral("Result"), QStringLiteral("Fail"), QColor(210, 0, 0));
    dialog->setFieldTextColor(QStringLiteral("Result"), QStringLiteral("Success"), QColor(0, 150, 0));
}
} // namespace

PneumaticValvePressureSettingWidget::PneumaticValvePressureSettingWidget(QWidget *parent)
    : SettingWidget(parent)
    , m_comboBox(nullptr)
    , m_pressureSpinBox(nullptr)
    , m_pressureSetBtn(nullptr)
    , m_pressureSetAllBtn(nullptr)
    , m_qrcodeItem(nullptr)
    , m_pressureItem(nullptr)
{
    setTitle("Pneumatic Valve Pressure Configuration");
    initUI();
}

PneumaticValvePressureSettingWidget::~PneumaticValvePressureSettingWidget()
{
}

// ============================================================
// UI 初始化
// ============================================================

void PneumaticValvePressureSettingWidget::initUI()
{
    initQrcodeItem();
    initPressureItem();
}

void PneumaticValvePressureSettingWidget::initQrcodeItem()
{
    m_qrcodeItem = new SettingItemWidget(this);
    m_qrcodeItem->setTitle("Target Device");
    m_qrcodeItem->setTip("Target device QRCode (numeric, used by Set button)");

    // m_qrcodeSpinBox = new QSpinBox(m_qrcodeItem);
    // m_qrcodeSpinBox->setRange(0, 99999);
    // m_qrcodeSpinBox->setFixedWidth(160);

    m_comboBox = new QComboBox(m_qrcodeItem);
    m_comboBox->setFixedWidth(160);

    // const QStringList qrcodes = SharedData::getAllQrcodes();
    // if (!qrcodes.isEmpty()) {
    //     bool ok = false;
    //     const int v = qrcodes.first().toInt(&ok);
    //     if (ok) m_qrcodeSpinBox->setValue(v);
    //     else qWarning() << "[ui][PneumaticValvePressureSettingWidget] qrcode 转换 int 失败:" << qrcodes.first();
    // }

    for (auto qrcode : OHBDeviceConfig::getInstance().readMasterDevices())
    {
        m_comboBox->addItem(qrcode);
    }

    // m_qrcodeItem->addWidget("qrcode_spin", m_qrcodeSpinBox);
    m_qrcodeItem->addWidget("qrcode_spin", m_comboBox);
    addItem(m_qrcodeItem);
}

void PneumaticValvePressureSettingWidget::initPressureItem()
{
    m_pressureItem = new SettingItemWidget(this);
    m_pressureItem->setTitle("Pneumatic Valve Pressure");
    m_pressureItem->setTip("Set pneumatic valve pressure (0~1000 bar). Register value = pressure x 10000");

    m_pressureSpinBox = new QSpinBox(m_pressureItem);
    m_pressureSpinBox->setRange(0, 1000);
    m_pressureSpinBox->setValue(5);
    m_pressureSpinBox->setSuffix(" bar");
    m_pressureSpinBox->setFixedWidth(120);
    m_pressureItem->addWidget("pressure_spin", m_pressureSpinBox);

    m_pressureSetBtn = new QPushButton("Set", m_pressureItem);
    m_pressureItem->addWidget("pressure_set_btn", m_pressureSetBtn);
    connect(m_pressureSetBtn, &QPushButton::clicked,
            this, &PneumaticValvePressureSettingWidget::onSetBtnClicked);

    m_pressureSetAllBtn = new QPushButton("Set All", m_pressureItem);
    m_pressureItem->addWidget("pressure_set_all_btn", m_pressureSetAllBtn);
    connect(m_pressureSetAllBtn, &QPushButton::clicked,
            this, &PneumaticValvePressureSettingWidget::onSetAllBtnClicked);

    addItem(m_pressureItem);
}

// ============================================================
// 槽函数
// ============================================================

void PneumaticValvePressureSettingWidget::onSetBtnClicked()
{
    // const QString qrcode = QString::number(m_qrcodeSpinBox->value());
    const QString qrcode = m_comboBox->currentText();
    const double pressureBar = m_pressureSpinBox->value();
    submitPressureTask(QStringList{qrcode}, pressureBar);
}

void PneumaticValvePressureSettingWidget::onSetAllBtnClicked()
{
    QStringList qrcodes;
    for (auto qrcode : OHBDeviceConfig::getInstance().readMasterDevices())
    {
        qrcodes.append(qrcode);
    }
    if (qrcodes.isEmpty()) {
        QMessageBox::warning(this, "Set Failed", "No target device available");
        return;
    }

    const double pressureBar = m_pressureSpinBox->value();
    submitPressureTask(qrcodes, pressureBar);
}

// ============================================================
// 内部辅助
// ============================================================

void PneumaticValvePressureSettingWidget::submitPressureTask(const QStringList &qrcodes,
                                                             double pressureBar)
{
    const QVector<QString> qrcodeVec(qrcodes.begin(), qrcodes.end());

    auto *task = new SetPneumaticValvePressureTask(qrcodeVec, pressureBar);

    m_pressureItem->setStatusWaiting();
    // 任务运行期间禁用两个 Set 按钮，避免并发任务提交
    setAllSetButtonsEnabled(false);

    connect(task, &SetPneumaticValvePressureTask::allFinished,
            this, [this, qrcodes](bool /*allSuccess*/, int successCount,
                         QStringList failedQrCodes, double pressureBarFinal) {
                // 只要有一台失败即视为本次任务失败
                const bool hasFailure = !failedQrCodes.isEmpty();
                const bool showDeviceTable = qrcodes.size() > 1 || failedQrCodes.size() > 1;
                if (!hasFailure) {
                    m_pressureItem->setStatusOK();
                    if (showDeviceTable) {
                        auto *dialog = ModalTableDialog::showAsync(
                            this,
                            QString("Pneumatic Valve Pressure=%1 bar Result").arg(pressureBarFinal),
                            QStringList{"QRCode", "Setting", "Result"},
                            buildDeviceResultRows(qrcodes, failedQrCodes, true, pressureBarFinal));
                        applyResultColors(dialog);
                    } else {
                        auto *mb = new QMessageBox(QMessageBox::Information,
                                                  "Set Succeeded",
                                                  QString("Successfully set Pneumatic Valve Pressure to [%1 bar] on %2 device(s)")
                                                      .arg(pressureBarFinal).arg(successCount),
                                                  QMessageBox::Ok,
                                                  this);
                        mb->setAttribute(Qt::WA_DeleteOnClose);
                        mb->setModal(false);
                        mb->setWindowModality(Qt::NonModal);
                        mb->show();
                    }
                } else {
                    m_pressureItem->setStatusFailed();
                    if (showDeviceTable) {
                        auto *dialog = ModalTableDialog::showAsync(
                            this,
                            QString("Pneumatic Valve Pressure=%1 bar Result").arg(pressureBarFinal),
                            QStringList{"QRCode", "Setting", "Result"},
                            buildDeviceResultRows(qrcodes, failedQrCodes, false, pressureBarFinal));
                        applyResultColors(dialog);
                    } else {
                        const QString failList = failedQrCodes.join(", ");
                        auto *mb = new QMessageBox(QMessageBox::Warning,
                                                  "Set Failed",
                                                  QString("Failed to set Pneumatic Valve Pressure to [%1 bar] on %2 device(s):\n%3")
                                                      .arg(pressureBarFinal)
                                                      .arg(failedQrCodes.count())
                                                      .arg(failList),
                                                  QMessageBox::Ok,
                                                  this);
                        mb->setAttribute(Qt::WA_DeleteOnClose);
                        mb->setModal(false);
                        mb->setWindowModality(Qt::NonModal);
                        mb->show();
                    }
                }
                setAllSetButtonsEnabled(true);
            });

    Scheduler::instance()->submitTask(task);

    qDebug() << "[ui][PneumaticValvePressureSettingWidget][submitPressureTask]：提交任务 设备数="
             << qrcodes.size() << "压力=" << pressureBar << "bar";
    LoggerManager::getInstance()->log(AppLogger::SystemLoggerPath().toStdString(), Level::INFO,
        QString("[ui][PneumaticValvePressureSettingWidget][submitPressureTask]：提交任务 设备数=%1 压力=%2bar")
            .arg(qrcodes.size()).arg(pressureBar).toStdString());
}

void PneumaticValvePressureSettingWidget::setAllSetButtonsEnabled(bool enabled)
{
    if (m_pressureSetBtn) {
        m_pressureSetBtn->setEnabled(enabled);
    }
    if (m_pressureSetAllBtn) {
        m_pressureSetAllBtn->setEnabled(enabled);
    }
}
