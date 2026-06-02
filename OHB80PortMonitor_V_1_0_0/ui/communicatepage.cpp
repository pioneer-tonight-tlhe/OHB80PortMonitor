#include "communicatepage.h"
#include "ui_communicatepage.h"
#include "app/shareddata.h"
#include "scheduler/tasks/monitor_data_task/monitor_data_task.h"
#include "logdatabases/databasemanager.h"
#include "logdatabases/communicatelogdb/communicatelogdbcon.h"
#include "usermanager.h"

#include <QJsonObject>
#include <QDateTime>

CommunicatePage::CommunicatePage(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::CommunicatePage)
{
    ui->setupUi(this);
    initCommLoggerWidget();

    // 连接 MonitorDataTask 的通讯完成信号
    MonitorDataTask* monitorTask = SharedData::getMonitorDataTask();
    if (monitorTask) {
        connect(monitorTask, &MonitorDataTask::communicationCompleted,
                this, &CommunicatePage::onCommunicationCompleted,
                Qt::QueuedConnection);
    }

    // 连接权限变化信号，更新 live log 列可见性
    connect(UserManager::instance(), &UserManager::permissionChanged,
            this, &CommunicatePage::onPermissionChanged);
}

CommunicatePage::~CommunicatePage()
{
    delete ui;
}

void CommunicatePage::initCommLoggerWidget()
{
    // 新 ComunicateLogWidget 的 UI 选项（QRCode 范围 / Command ID 下拉等）
    // 由 widget 内部统一处理；老 widget 的 setRootPath / setMaxFileBytes /
    // initQrcodeList / setCommandIds / initialize 等接口已废弃，
    // 持久化改由 LogDB::CommunicateLogDBCon 接管，live log 通过其
    // recordInserted 信号自动渲染。
    ui->commLoggerWidget->initUi();
}

void CommunicatePage::onCommunicationCompleted(ModbusCommand cmd, QString masterId, QString description)
{
    // 计算响应时间（ms）：发送到接收的时间差
    qint64 responseTimeMs = 0;
    if (cmd.sentMs > 0 && cmd.responseMs > 0) {
        responseTimeMs = cmd.responseMs - cmd.sentMs;
    }

    // 将原始数据帧转为十六进制字符串
    QString sendFrameHex = cmd.request.rawBytes.toHex(' ').toUpper();
    QString recvFrameHex = cmd.response.rawBytes.toHex(' ').toUpper();

    // 将发送时刻格式化为可读字符串
    QString sentTimeStr = cmd.sentMs > 0
        ? QDateTime::fromMSecsSinceEpoch(cmd.sentMs).toString(QStringLiteral("yyyy-MM-dd HH:mm:ss"))
        : QStringLiteral("-");

    QString respTimeStr = cmd.responseMs > 0
        ? QDateTime::fromMSecsSinceEpoch(cmd.responseMs).toString(QStringLiteral("yyyy-MM-dd HH:mm:ss"))
        : QString();

    // exec_status：0=Success / 1=Timeout / 2=Send Failed
    int execStatus = 2;
    if (cmd.received) {
        execStatus = 0;
    } else if (cmd.timedOut) {
        execStatus = 1;
    }

    const int retryCount = qMax(0, cmd.sendCount - 1);

    // 调用 ComunicateLogWidget::writeLog 更新实时日志（固定行数表格，按 qrcode 索引）
    // description 由 MonitorDataTask::onCommunicationRecorded 解析并传递
    ui->commLoggerWidget->writeLog(
        masterId,
        sentTimeStr,
        respTimeStr,
        cmd.id,
        QString::number(responseTimeMs),
        QString::number(execStatus),
        QString::number(retryCount),
        sendFrameHex,
        recvFrameHex,
        description
    );

    // 数据库写入已移至 MonitorDataTask::onCommunicationRecorded 中完成
}

void CommunicatePage::onPermissionChanged(UserPermission permission)
{
    Q_UNUSED(permission);
    // 更新 live log 列可见性
    ui->commLoggerWidget->updateColumnVisibility();
}
