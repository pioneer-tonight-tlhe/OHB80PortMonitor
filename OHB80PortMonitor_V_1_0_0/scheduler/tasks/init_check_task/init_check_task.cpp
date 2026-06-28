#include "init_check_task.h"
#include "modbustcpmastermanager/modbustcpmastermanager.h"
#include "modbustcpmastermanager/modbustcpmaster/modbustcpmaster.h"
#include "modbustcpmastermanager/modbustcpmaster/initialcommandissuer.h"
#include "app/applogger.h"
#include "app/shareddata.h"
#include "loggermanager.h"
#include "scheduler/tasks/operation_dispatch_task/operation_dispatch_task.h"

#include <QDebug>

InitCheckTask::InitCheckTask(QObject* parent)
    : SchedulerTask(parent)
{
    qDebug() << "=============================InitCheckTask 调度任务开始=============================";
    LoggerManager::getInstance()->log(m_taskLogPath, Level::INFO,
        "=============================InitCheckTask 调度任务开始=============================");
}

InitCheckTask::~InitCheckTask()
{
    qDebug() << "=============================InitCheckTask 调度任务结束=============================";
    LoggerManager::getInstance()->log(m_taskLogPath, Level::INFO,
        "=============================InitCheckTask 调度任务结束=============================");
}

void InitCheckTask::start()
{
    setState(Running);
    m_stopped = false;
    m_totalCount = 0;
    m_completedCount = 0;
    m_successCount = 0;
    m_failedMasterIds.clear();
    m_errorMsgMap.clear();
    m_connections.clear();

    ModbusTcpMasterManager &manager = ModbusTcpMasterManager::instance();
    QStringList ids = manager.masterIds();

    qDebug() << "[Scheduler][InitCheckTask][start] 发现" << ids.size() << "个已注册的设备:" << ids.join(", ");
    LoggerManager::getInstance()->log(m_taskLogPath, Level::INFO,
        QString("[Scheduler][InitCheckTask][start] 发现 %1 个已注册的设备: %2").arg(ids.size()).arg(ids.join(", ")).toStdString());

    for (const QString& id : ids) {
        ModbusTcpMaster *master = manager.getMaster(id);
        if (!master) {
            qWarning() << "[Scheduler][InitCheckTask][start] 无法获取设备" << id << "的 Master 对象，跳过";
            LoggerManager::getInstance()->log(m_taskLogPath, Level::WARN,
                QString("[Scheduler][InitCheckTask][start] 无法获取设备 %1 的 Master 对象，跳过").arg(id).toStdString());
            continue;
        }

        InitialCommandIssuer *issuer = master->initialIssuer();
        if (!issuer) {
            qWarning() << "[Scheduler][InitCheckTask][start] 设备" << id << "的 InitialCommandIssuer 为空，跳过";
            LoggerManager::getInstance()->log(m_taskLogPath, Level::WARN,
                QString("[Scheduler][InitCheckTask][start] 设备 %1 的 InitialCommandIssuer 为空，跳过").arg(id).toStdString());
            continue;
        }

        // 通过 lambda 捕获 masterId，转发到 onInitialFinished
        auto conn = connect(issuer, &InitialCommandIssuer::finish,
                            this, [this, id](bool isOk, QStringList errorMsgList) {
                                onInitialFinished(isOk, errorMsgList, id);
                            }, Qt::QueuedConnection);
        m_connections.append(conn);
        m_totalCount++;
        qDebug() << "[Scheduler][InitCheckTask][start] 已连接设备" << id << "的初始化完成信号";
        LoggerManager::getInstance()->log(m_taskLogPath, Level::DEBUG,
            QString("[Scheduler][InitCheckTask][start] 已连接设备 %1 的初始化完成信号").arg(id).toStdString());
    }

    if (m_totalCount == 0) {
        setState(Failed);
        emit allFinished(false, 0, 0, {});
        emit finished(false, "InitCheckTask: 没有可监听的 InitialCommandIssuer");
        LoggerManager::getInstance()->log(m_taskLogPath, Level::WARN,
            "[Scheduler][InitCheckTask][start] 没有可监听的 InitialCommandIssuer");
        return;
    }

    qDebug() << "[Scheduler][InitCheckTask][start] 启动，等待" << m_totalCount << "个设备完成初始化";
    LoggerManager::getInstance()->log(m_taskLogPath, Level::INFO,
        QString("[Scheduler][InitCheckTask][start] 启动，等待 %1 个设备完成初始化").arg(m_totalCount).toStdString());
    emit progress(0, QString("等待 %1 个设备完成初始化").arg(m_totalCount));
}

void InitCheckTask::stop()
{
    qDebug() << "[Scheduler][InitCheckTask][stop] 停止任务，当前进度:" << m_completedCount << "/" << m_totalCount;
    LoggerManager::getInstance()->log(m_taskLogPath, Level::INFO,
        QString("[Scheduler][InitCheckTask][stop] 停止任务，当前进度: %1/%2").arg(m_completedCount).arg(m_totalCount).toStdString());
    m_stopped = true;
    disconnectAll();
    setState(Cancelled);
    emit finished(false, "InitCheckTask: 任务被取消");
    LoggerManager::getInstance()->log(m_taskLogPath, Level::INFO,
        "[Scheduler][InitCheckTask][stop] 任务被取消");
}

void InitCheckTask::onInitialFinished(bool isOk, QStringList errorMsgList, const QString& masterId)
{
    if (m_stopped) return;

    if (isOk) {
        m_successCount++;
        qDebug() << "[Scheduler][InitCheckTask][onInitialFinished] 设备" << masterId << "初始化成功 (" << m_completedCount + 1 << "/" << m_totalCount << ")";
        LoggerManager::getInstance()->log(m_taskLogPath, Level::INFO,
            QString("[Scheduler][InitCheckTask][onInitialFinished] 设备 %1 初始化成功 (%2/%3)").arg(masterId).arg(m_completedCount + 1).arg(m_totalCount).toStdString());

        if (auto* opTask = SharedData::getOperationDispatchTask()) {
            opTask->log(OperationDispatchTask::MsgType::Message,
                        QString("Initial commands succeeded: QRCode=%1").arg(masterId),
                        0);
        }
    } else {
        m_failedMasterIds.append(masterId);
        if (errorMsgList.isEmpty()) {
            errorMsgList << QString("Initial commands failed: QRCode=%1, reason=unknown").arg(masterId);
        }
        m_errorMsgMap[masterId] = errorMsgList;

        qWarning() << "[Scheduler][InitCheckTask][onInitialFinished] 设备" << masterId
                   << "初始化存在错误:" << errorMsgList.join("; ")
                   << "(" << m_completedCount + 1 << "/" << m_totalCount << ")";
        LoggerManager::getInstance()->log(m_taskLogPath, Level::WARN,
            QString("[Scheduler][InitCheckTask][onInitialFinished] 设备 %1 初始化存在 %2 条错误: %3 (%4/%5)")
                .arg(masterId).arg(errorMsgList.size()).arg(errorMsgList.join("; ")).arg(m_completedCount + 1).arg(m_totalCount).toStdString());

        for (int i = 0; i < errorMsgList.size(); ++i) {
            const QString errorLine = QString("[Scheduler][InitCheckTask][onInitialFinished] 设备 %1 初始化错误 %2/%3: %4")
                                          .arg(masterId)
                                          .arg(i + 1)
                                          .arg(errorMsgList.size())
                                          .arg(errorMsgList.at(i));
            qWarning().noquote() << errorLine;
            LoggerManager::getInstance()->log(m_taskLogPath, Level::WARN, errorLine.toStdString());
        }

        if (auto* opTask = SharedData::getOperationDispatchTask()) {
            opTask->log(OperationDispatchTask::MsgType::Warn,
                        QString("Initial commands failed: QRCode=%1, errorCount=%2")
                            .arg(masterId)
                            .arg(errorMsgList.size()),
                        0);
            for (const QString& errorMsg : qAsConst(errorMsgList)) {
                opTask->log(OperationDispatchTask::MsgType::Warn, errorMsg, 0);
            }
        }
    }

    m_completedCount++;
    int percent = m_totalCount > 0 ? (m_completedCount * 100 / m_totalCount) : 100;
    emit progress(percent, QString("已完成 %1/%2").arg(m_completedCount).arg(m_totalCount));

    checkAllFinished();
}

void InitCheckTask::checkAllFinished()
{
    if (m_completedCount < m_totalCount) return;

    qDebug() << "[Scheduler][InitCheckTask][checkAllFinished] 全部设备初始化检查完成，汇总结果:";
    qDebug() << "  - 总设备数:" << m_totalCount;
    qDebug() << "  - 成功设备数:" << m_successCount;
    qDebug() << "  - 失败设备数:" << m_failedMasterIds.count();
    if (!m_failedMasterIds.isEmpty()) {
        qDebug() << "  - 失败设备ID:" << m_failedMasterIds.join(", ");
    }
    LoggerManager::getInstance()->log(m_taskLogPath, Level::INFO,
        QString("[Scheduler][InitCheckTask][checkAllFinished] 全部设备初始化检查完成，汇总结果: 总设备数=%1, 成功=%2, 失败=%3")
            .arg(m_totalCount).arg(m_successCount).arg(m_failedMasterIds.count()).toStdString());

    disconnectAll();
    const bool allSuccess = m_failedMasterIds.isEmpty();
    setState(allSuccess ? Finished : Failed);

    emit allFinished(allSuccess, m_successCount, m_failedMasterIds.count(), m_failedMasterIds);

    QString msg = allSuccess
        ? QString("全部 %1 台设备初始化成功").arg(m_totalCount)
        : QString("初始化完成：%1 台成功，%2 台存在初始化错误（%3）")
              .arg(m_successCount)
              .arg(m_failedMasterIds.count())
              .arg(m_failedMasterIds.join(", "));

    emit finished(allSuccess, msg);
    LoggerManager::getInstance()->log(m_taskLogPath,
        allSuccess ? Level::INFO : Level::WARN,
        QString("[Scheduler][InitCheckTask][checkAllFinished] %1").arg(msg).toStdString());
}

void InitCheckTask::disconnectAll()
{
    qDebug() << "[Scheduler][InitCheckTask][disconnectAll] 断开所有信号连接，连接数:" << m_connections.size();
    LoggerManager::getInstance()->log(m_taskLogPath, Level::DEBUG,
        QString("[Scheduler][InitCheckTask][disconnectAll] 断开所有信号连接，连接数: %1").arg(m_connections.size()).toStdString());
    for (const QMetaObject::Connection& conn : qAsConst(m_connections))
        QObject::disconnect(conn);
    m_connections.clear();
}
