#include "humidityoffsetsettingwidget.h"
#include "../settingwidget/settingitemwidget.h"
#include "modaltabledialog.h"
#include "scheduler/scheduler.h"
#include "scheduler/tasks/set_humidity_offset_task/set_humidity_offset_task.h"
#include "app/shareddata.h"
#include "app/applogger.h"
#include "loggermanager.h"

#include <QColor>
#include <QDebug>
#include <QList>
#include <QMessageBox>
#include <QStringList>
#include <QVector>

namespace {
QList<QStringList> buildDeviceResultRows(const QStringList &targetQrCodes,
                                         const QStringList &failedQrCodes,
                                         bool allSuccess,
                                         const QString &fieldName,
                                         double valuePct)
{
    QStringList qrcodes = targetQrCodes;
    for (const QString &qrCode : failedQrCodes) {
        if (!qrcodes.contains(qrCode)) {
            qrcodes.append(qrCode);
        }
    }

    // 格式化设置值（带单位）
    QString settingValueStr;
    if (fieldName == "Threshold") {
        settingValueStr = QString("Set Threshold %1%").arg(valuePct);
    } else if (fieldName == "Offset") {
        settingValueStr = QString("Set Offset %1%").arg(valuePct);
    } else {
        settingValueStr = QString("Set %1 %2%").arg(fieldName).arg(valuePct);
    }

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

HumidityOffsetSettingWidget::HumidityOffsetSettingWidget(QWidget *parent)
    : SettingWidget(parent)
    , m_qrcodeSpinBox(nullptr)
    , m_thresholdSpinBox(nullptr)
    , m_offsetSpinBox(nullptr)
    , m_thresholdSetBtn(nullptr)
    , m_thresholdSetAllBtn(nullptr)
    , m_offsetSetBtn(nullptr)
    , m_offsetSetAllBtn(nullptr)
    , m_qrcodeItem(nullptr)
    , m_thresholdItem(nullptr)
    , m_offsetItem(nullptr)
{
    setTitle("Humidity Offset Configuration");
    initUI();
}

HumidityOffsetSettingWidget::~HumidityOffsetSettingWidget() = default;

// ============================================================
// UI 初始化
// ============================================================

void HumidityOffsetSettingWidget::initUI()
{
    initQrcodeItem();
    initThresholdItem();
    initOffsetItem();
}

void HumidityOffsetSettingWidget::initQrcodeItem()
{
    m_qrcodeItem = new SettingItemWidget(this);
    m_qrcodeItem->setTitle("Target Device");
    m_qrcodeItem->setTip("Target device QRCode (numeric) for humidity offset setting");

    m_qrcodeSpinBox = new QSpinBox(m_qrcodeItem);
    m_qrcodeSpinBox->setRange(0, 2147483647);
    m_qrcodeSpinBox->setFixedWidth(160);

    // 初始值：SharedData::getAllQrcodes() 第一个
    const QStringList qrcodes = SharedData::getAllQrcodes();
    if (!qrcodes.isEmpty()) {
        bool ok = false;
        const int v = qrcodes.first().toInt(&ok);
        if (ok) {
            m_qrcodeSpinBox->setValue(v);
        } else {
            qWarning() << "[ui][HumidityOffsetSettingWidget] qrcode 转换 int 失败:" << qrcodes.first();
        }
    }

    m_qrcodeItem->addWidget("qrcode_spin", m_qrcodeSpinBox);
    addItem(m_qrcodeItem);
}

void HumidityOffsetSettingWidget::initThresholdItem()
{
    m_thresholdItem = new SettingItemWidget(this);
    m_thresholdItem->setTitle("Humidity Offset Threshold");
    m_thresholdItem->setTip("Set humidity calibration trigger threshold (0~100 %). Register value = pct x 100");

    m_thresholdSpinBox = new QDoubleSpinBox(m_thresholdItem);
    m_thresholdSpinBox->setRange(0.0, 100.0);
    m_thresholdSpinBox->setDecimals(2);
    m_thresholdSpinBox->setSingleStep(0.1);
    m_thresholdSpinBox->setValue(5.0);
    m_thresholdSpinBox->setSuffix(" %");
    m_thresholdSpinBox->setFixedWidth(120);
    m_thresholdItem->addWidget("threshold_spin", m_thresholdSpinBox);

    m_thresholdSetBtn = new QPushButton("Set", m_thresholdItem);
    m_thresholdItem->addWidget("threshold_set_btn", m_thresholdSetBtn);
    connect(m_thresholdSetBtn, &QPushButton::clicked,
            this, &HumidityOffsetSettingWidget::onSetThresholdClicked);

    m_thresholdSetAllBtn = new QPushButton("Set All", m_thresholdItem);
    m_thresholdItem->addWidget("threshold_set_all_btn", m_thresholdSetAllBtn);
    connect(m_thresholdSetAllBtn, &QPushButton::clicked,
            this, &HumidityOffsetSettingWidget::onSetThresholdAllClicked);

    addItem(m_thresholdItem);
}

void HumidityOffsetSettingWidget::initOffsetItem()
{
    m_offsetItem = new SettingItemWidget(this);
    m_offsetItem->setTitle("Humidity Offset");
    m_offsetItem->setTip("Set humidity offset parameter (0~100 %). Register value = pct x 100");

    m_offsetSpinBox = new QDoubleSpinBox(m_offsetItem);
    m_offsetSpinBox->setRange(0.0, 100.0);
    m_offsetSpinBox->setDecimals(2);
    m_offsetSpinBox->setSingleStep(0.1);
    m_offsetSpinBox->setValue(0.0);
    m_offsetSpinBox->setSuffix(" %");
    m_offsetSpinBox->setFixedWidth(120);
    m_offsetItem->addWidget("offset_spin", m_offsetSpinBox);

    m_offsetSetBtn = new QPushButton("Set", m_offsetItem);
    m_offsetItem->addWidget("offset_set_btn", m_offsetSetBtn);
    connect(m_offsetSetBtn, &QPushButton::clicked,
            this, &HumidityOffsetSettingWidget::onSetOffsetClicked);

    m_offsetSetAllBtn = new QPushButton("Set All", m_offsetItem);
    m_offsetItem->addWidget("offset_set_all_btn", m_offsetSetAllBtn);
    connect(m_offsetSetAllBtn, &QPushButton::clicked,
            this, &HumidityOffsetSettingWidget::onSetOffsetAllClicked);

    addItem(m_offsetItem);
}

// ============================================================
// 槽函数
// ============================================================

void HumidityOffsetSettingWidget::onSetThresholdClicked()
{
    const QString qrcode = QString::number(m_qrcodeSpinBox->value());
    submitTask(QStringList{qrcode}, /*isThreshold=*/true,
               m_thresholdSpinBox->value(), m_thresholdItem);
}

void HumidityOffsetSettingWidget::onSetThresholdAllClicked()
{
    const QStringList qrcodes = SharedData::getAllQrcodes();
    if (qrcodes.isEmpty()) {
        QMessageBox::warning(this, "Set Failed", "No target device available");
        return;
    }
    submitTask(qrcodes, /*isThreshold=*/true,
               m_thresholdSpinBox->value(), m_thresholdItem);
}

void HumidityOffsetSettingWidget::onSetOffsetClicked()
{
    const QString qrcode = QString::number(m_qrcodeSpinBox->value());
    submitTask(QStringList{qrcode}, /*isThreshold=*/false,
               m_offsetSpinBox->value(), m_offsetItem);
}

void HumidityOffsetSettingWidget::onSetOffsetAllClicked()
{
    const QStringList qrcodes = SharedData::getAllQrcodes();
    if (qrcodes.isEmpty()) {
        QMessageBox::warning(this, "Set Failed", "No target device available");
        return;
    }
    submitTask(qrcodes, /*isThreshold=*/false,
               m_offsetSpinBox->value(), m_offsetItem);
}

// ============================================================
// 内部辅助
// ============================================================

void HumidityOffsetSettingWidget::submitTask(const QStringList &qrcodes,
                                             bool isThreshold,
                                             double valuePct,
                                             SettingItemWidget *targetItem)
{
    const QVector<QString> qrcodeVec(qrcodes.begin(), qrcodes.end());
    auto *task = new SetHumidityOffsetTask(qrcodeVec);
    if (isThreshold)
        task->setThreshold(valuePct);
    else
        task->setOffset(valuePct);

    targetItem->setStatusWaiting();
    // 任务运行期间禁用四个 Set 按钮，避免并发任务提交
    setAllSetButtonsEnabled(false);

    const QString fieldName = isThreshold ? "Threshold" : "Offset";

    connect(task, &SetHumidityOffsetTask::deviceRetrying, this,
            [targetItem](const QString &qrCode, int retryCount, int maxRetry) {
                targetItem->setStatusWaiting(QString("Retrying %1 (%2/%3)")
                    .arg(qrCode)
                    .arg(retryCount)
                    .arg(maxRetry));
            },
            Qt::QueuedConnection);

    connect(task, &SetHumidityOffsetTask::allFinished,
            this, [this, targetItem, fieldName, valuePct, qrcodes]
                  (bool allSuccess, int successCount,
                   QStringList failedQrCodes,
                   bool /*thresholdSet*/, double /*thresholdPct*/,
                   bool /*offsetSet*/,    double /*offsetPct*/) {
                const bool hasFailure = !allSuccess || !failedQrCodes.isEmpty();
                const bool showDeviceTable = qrcodes.size() > 1 || failedQrCodes.size() > 1;
                if (!hasFailure) {
                    targetItem->setStatusOK();
                    if (showDeviceTable) {
                        auto *dialog = ModalTableDialog::showAsync(
                            this,
                            QString("Humidity %1=%2% Result").arg(fieldName).arg(valuePct),
                            QStringList{"QRCode", "Setting", "Result"},
                            buildDeviceResultRows(qrcodes, failedQrCodes, allSuccess, fieldName, valuePct));
                        applyResultColors(dialog);
                    } else {
                        auto *mb = new QMessageBox(QMessageBox::Information,
                                                  "Set Succeeded",
                                                  QString("Successfully set Humidity %1 to [%2 %] on %3 device(s)")
                                                      .arg(fieldName).arg(valuePct).arg(successCount),
                                                  QMessageBox::Ok,
                                                  this);
                        mb->setAttribute(Qt::WA_DeleteOnClose);
                        // 显式非模态：避免 QDialog::open() 自动改成 WindowModal 阻塞主窗口
                        mb->setModal(false);
                        mb->setWindowModality(Qt::NonModal);
                        mb->show();
                    }
                } else {
                    targetItem->setStatusFailed();
                    if (showDeviceTable) {
                        auto *dialog = ModalTableDialog::showAsync(
                            this,
                            QString("Humidity %1=%2% Result").arg(fieldName).arg(valuePct),
                            QStringList{"QRCode", "Setting", "Result"},
                            buildDeviceResultRows(qrcodes, failedQrCodes, allSuccess, fieldName, valuePct));
                        applyResultColors(dialog);
                    } else {
                        const QString failList = failedQrCodes.join(", ");
                        auto *mb = new QMessageBox(QMessageBox::Warning,
                                                  "Set Failed",
                                                  QString("Failed to set Humidity %1=[%2 %] on %3 device(s):\n%4")
                                                      .arg(fieldName).arg(valuePct)
                                                      .arg(failedQrCodes.count()).arg(failList),
                                                  QMessageBox::Ok,
                                                  this);
                        mb->setAttribute(Qt::WA_DeleteOnClose);
                        // 显式非模态：避免 QDialog::open() 自动改成 WindowModal 阻塞主窗口
                        mb->setModal(false);
                        mb->setWindowModality(Qt::NonModal);
                        mb->show();
                    }
                }
                setAllSetButtonsEnabled(true);
            });

    Scheduler::instance()->submitTask(task);

    qDebug() << "[ui][HumidityOffsetSettingWidget][submitTask]：提交任务"
             << "field=" << fieldName << "设备数=" << qrcodes.size() << "value=" << valuePct << "%";
    LoggerManager::getInstance()->log(AppLogger::SystemLoggerPath().toStdString(), Level::INFO,
        QString("[ui][HumidityOffsetSettingWidget][submitTask]：提交任务 field=%1 设备数=%2 value=%3%%")
            .arg(fieldName).arg(qrcodes.size()).arg(valuePct).toStdString());
}

void HumidityOffsetSettingWidget::setAllSetButtonsEnabled(bool enabled)
{
    if (m_thresholdSetBtn) {
        m_thresholdSetBtn->setEnabled(enabled);
    }
    if (m_thresholdSetAllBtn) {
        m_thresholdSetAllBtn->setEnabled(enabled);
    }
    if (m_offsetSetBtn) {
        m_offsetSetBtn->setEnabled(enabled);
    }
    if (m_offsetSetAllBtn) {
        m_offsetSetAllBtn->setEnabled(enabled);
    }
}
