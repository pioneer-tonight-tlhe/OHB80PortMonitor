#include "vefcflowunitmediumstatuswidget.h"
#include "../settingwidget/settingitemwidget.h"
#include "modaltabledialog.h"
#include "scheduler/scheduler.h"
#include "tasks/read_vefc_flow_unit_medium_status_task/read_vefc_flow_unit_medium_status_task.h"
#include "app/shareddata.h"
#include "app/applogger.h"
#include "loggermanager.h"
#include "modbustcpmastermanager/modbustcpmastermanager.h"

#include <QDebug>
#include <QMessageBox>
#include <QColor>
#include <QStringList>
#include <QVector>

VEFCFlowUnitMediumStatusWidget::VEFCFlowUnitMediumStatusWidget(QWidget *parent)
    : SettingWidget(parent)
{
    setTitle("VEFC Flow Unit / Medium Status");
    initUI();
}

VEFCFlowUnitMediumStatusWidget::~VEFCFlowUnitMediumStatusWidget() = default;

// ============================================================
// UI
// ============================================================

void VEFCFlowUnitMediumStatusWidget::initUI()
{
    initQrcodeItem();
    initReadItem();
}

void VEFCFlowUnitMediumStatusWidget::initQrcodeItem()
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
        else qWarning() << "[ui][VEFCFlowUnitMediumStatusWidget] qrcode 转换 int 失败:" << qrcodes.first();
    }

    m_qrcodeItem->addWidget("qrcode_spin", m_qrcodeSpinBox);
    addItem(m_qrcodeItem);
}

void VEFCFlowUnitMediumStatusWidget::initReadItem()
{
    m_readItem = new SettingItemWidget(this);
    m_readItem->setTitle("Flow Unit / Medium Status");
    m_readItem->setTip("Read VEFC flow unit & medium config status. Default unit=L/Min, default medium=CDA");

    auto *readBtn = new QPushButton("Read", m_readItem);
    m_readItem->addWidget("status_read_btn", readBtn);
    connect(readBtn, &QPushButton::clicked,
            this, &VEFCFlowUnitMediumStatusWidget::onReadBtnClicked);

    auto *readAllBtn = new QPushButton("Read All", m_readItem);
    m_readItem->addWidget("status_read_all_btn", readAllBtn);
    connect(readAllBtn, &QPushButton::clicked,
            this, &VEFCFlowUnitMediumStatusWidget::onReadAllBtnClicked);

    addItem(m_readItem);
}

// ============================================================
// 槽
// ============================================================

void VEFCFlowUnitMediumStatusWidget::onReadBtnClicked()
{
    const QString qrcode = QString::number(m_qrcodeSpinBox->value());
    submitTask(QStringList{qrcode});
}

void VEFCFlowUnitMediumStatusWidget::onReadAllBtnClicked()
{
    const QStringList qrcodes = ModbusTcpMasterManager::instance().masterIds();
    if (qrcodes.isEmpty()) {
        QMessageBox::warning(this, "Read Failed", "No target device available");
        return;
    }
    submitTask(qrcodes);
}

// ============================================================
// 辅助函数
// ============================================================

namespace {
QList<QStringList> buildDeviceResultRows(const QList<ReadVEFCFlowUnitAndMediumStatusTask::DeviceStatus>& results)
{
    QList<QStringList> rows;
    for (const auto& st : results) {
        rows.append({
            st.qrcode,
            st.commFailed ? QStringLiteral("Failed") : QStringLiteral("OK"),
            st.unitOk ? QStringLiteral("OK") : QStringLiteral("Failed"),
            st.mediumOk ? QStringLiteral("OK") : QStringLiteral("Failed"),
            st.allOk() ? QStringLiteral("Success") : QStringLiteral("Failed")
        });
    }
    return rows;
}

void applyResultColors(ModalTableDialog *dialog)
{
    if (!dialog) return;
    dialog->setFieldTextColor(QStringLiteral("Communication"), QStringLiteral("Failed"), QColor(210, 0, 0));
    dialog->setFieldTextColor(QStringLiteral("Communication"), QStringLiteral("OK"), QColor(0, 150, 0));
    dialog->setFieldTextColor(QStringLiteral("Unit Status"), QStringLiteral("Failed"), QColor(210, 0, 0));
    dialog->setFieldTextColor(QStringLiteral("Unit Status"), QStringLiteral("OK"), QColor(0, 150, 0));
    dialog->setFieldTextColor(QStringLiteral("Medium Status"), QStringLiteral("Failed"), QColor(210, 0, 0));
    dialog->setFieldTextColor(QStringLiteral("Medium Status"), QStringLiteral("OK"), QColor(0, 150, 0));
    dialog->setFieldTextColor(QStringLiteral("Overall"), QStringLiteral("Failed"), QColor(210, 0, 0));
    dialog->setFieldTextColor(QStringLiteral("Overall"), QStringLiteral("Success"), QColor(0, 150, 0));
}
} // namespace

// ============================================================
// 提交任务
// ============================================================

void VEFCFlowUnitMediumStatusWidget::submitTask(const QStringList &qrcodes)
{
    using Task = ReadVEFCFlowUnitAndMediumStatusTask;
    const QVector<QString> qrcodeVec(qrcodes.begin(), qrcodes.end());
    auto *task = new Task(qrcodeVec);

    m_readItem->setStatusWaiting();

    connect(task, &Task::deviceRetrying, this,
            [this](const QString &qrCode, int retryCount, int maxRetry) {
                m_readItem->setStatusWaiting(QString("Retrying %1 (%2/%3)")
                    .arg(qrCode)
                    .arg(retryCount)
                    .arg(maxRetry));
            },
            Qt::QueuedConnection);

    connect(task, &Task::allFinished,
            this, [this](bool allSuccess, int successCount,
                         QList<Task::DeviceStatus> results) {
                if (allSuccess) {
                    m_readItem->setStatusOK();
                } else {
                    m_readItem->setStatusFailed();
                }

                auto *dialog = ModalTableDialog::showAsync(
                    this,
                    QString("VEFC Flow Unit/Medium Status Result"),
                    QStringList{"QRCode", "Communication", "Unit Status", "Medium Status", "Overall"},
                    buildDeviceResultRows(results));
                applyResultColors(dialog);
            });

    Scheduler::instance()->submitTask(task);

    qDebug() << "[ui][VEFCFlowUnitMediumStatusWidget][submitTask]：提交任务 设备数=" << qrcodes.size();
    LoggerManager::getInstance()->log(AppLogger::SystemLoggerPath().toStdString(), Level::INFO,
        QString("[ui][VEFCFlowUnitMediumStatusWidget][submitTask]：提交任务 设备数=%1")
            .arg(qrcodes.size()).toStdString());
}
