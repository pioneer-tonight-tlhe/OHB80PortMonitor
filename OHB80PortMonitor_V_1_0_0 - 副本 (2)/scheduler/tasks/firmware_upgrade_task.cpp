#include "firmware_upgrade_task.h"
#include "modbustcpmastermanager/modbustcpmastermanager.h"
#include "modbustcpmastermanager/modbustcpmaster/modbustcpmaster.h"
#include "modbustcpmastermanager/modbustcpmaster/modbuscommandsender.h"
#include "modbustcpmastermanager/modbuscommand/commandpool.h"
#include "modbustcpmastermanager/modbuscommand/modbuscommand.h"
#include "tool/binfilereader/binfilereader.h"
#include "loggermanager.h"
#include "app/applogger.h"
#include "app/shareddata.h"
#include "scheduler/tasks/operation_dispatch_task.h"
#include "logdatabases/databasemanager.h"
#include "logdatabases/communicatelogdb/communicatelogdbcon.h"
#include "modbustcpmastermanager/modbuscommand/commandresponseparser.h"
#include "usermanager/usermanager.h"
#include "defer/defer.h"
#include <QDebug>
#include <QDateTime>

FirmwareUpgradeTask::FirmwareUpgradeTask(const QStringList &deviceIds,
                                         const QString &binFilePath,
                                         QObject *parent)
    : SchedulerTask(parent)
    , m_deviceIds(deviceIds)
    , m_binFilePath(binFilePath)
{
    LoggerManager::getInstance()->log(m_taskLogPath, Level::INFO, "[FirmwareUpgradeTask] 固件升级任务已构造");
    LoggerManager::getInstance()->flush(m_taskLogPath);
}

FirmwareUpgradeTask::~FirmwareUpgradeTask()
{
    delete m_binFileReader;
}

void FirmwareUpgradeTask::start()
{
    // 使用 Defer 确保函数退出时刷新日志
    Tool::Defer defer([this]() {
        LoggerManager::getInstance()->flush(m_taskLogPath);
    });

    setState(Running);
    m_stopped      = false;
    m_finishedCount = 0;
    m_totalCount   = 0;
    m_upgraderMap.clear();

    qDebug() << "[Scheduler][FirmwareUpgradeTask][start] 固件升级任务启动";
    LoggerManager::getInstance()->log(m_taskLogPath, Level::INFO,
        "[FirmwareUpgradeTask][start] 固件升级任务启动");

    if (m_deviceIds.isEmpty()) {
        setState(Finished);
        qDebug() << "[Scheduler][FirmwareUpgradeTask][start] 没有待升级的设备";
        LoggerManager::getInstance()->log(m_taskLogPath, Level::WARN,
            "[FirmwareUpgradeTask][start] 没有待升级的设备");
        emit finished(false, "没有待升级的设备");
        return;
    }

    if (m_binFilePath.isEmpty()) {
        setState(Failed);
        qDebug() << "[Scheduler][FirmwareUpgradeTask][start] 未指定固件文件路径";
        LoggerManager::getInstance()->log(m_taskLogPath, Level::ERROR,
            "[FirmwareUpgradeTask][start] 未指定固件文件路径");
        emit finished(false, "未指定固件文件路径");
        return;
    }

    // 构建共享 BinFileReader，按 {248, 256} 规则分包
    m_binFileReader = new BinFileReader(this);
    m_binFileReader->setPacketSize(QVector<int>{248, 256});

    connect(m_binFileReader, &BinFileReader::sigReadFinished,
            this, &FirmwareUpgradeTask::onBinFileReadFinished);

    qDebug() << "[Scheduler][FirmwareUpgradeTask][start] 开始读取固件文件:" << m_binFilePath;
    LoggerManager::getInstance()->log(m_taskLogPath, Level::INFO,
        QString("[FirmwareUpgradeTask][start] 开始读取固件文件: %1").arg(m_binFilePath).toStdString());

    m_binFileReader->readBinFile(m_binFilePath);
}

void FirmwareUpgradeTask::stop()
{
    // 使用 Defer 确保函数退出时刷新日志
    Tool::Defer defer([this]() {
        LoggerManager::getInstance()->flush(m_taskLogPath);
    });

    m_stopped = true;

    qDebug() << "[Scheduler][FirmwareUpgradeTask][stop] 停止固件升级任务";
    LoggerManager::getInstance()->log(m_taskLogPath, Level::INFO,
        "[FirmwareUpgradeTask][stop] 停止固件升级任务");

    for (auto it = m_upgraderMap.constBegin(); it != m_upgraderMap.constEnd(); ++it) {
        FirmwareUpgrader *upgrader = it.value();
        if (upgrader && upgrader->isRunning()) {
            upgrader->stop();  // FirmwareUpgrader::stop() 已内置线程分派
        }
    }

    // 断开所有 WriteQRCode 相关的信号连接
    for (const QMetaObject::Connection &conn : qAsConst(m_qrCodeConnections)) {
        QObject::disconnect(conn);
    }
    m_qrCodeConnections.clear();

    setState(Cancelled);
    emit finished(false, "固件升级任务已取消");
}

void FirmwareUpgradeTask::onBinFileReadFinished(bool success, const QString &errorMsg)
{
    if (m_stopped) return;

    if (!success) {
        setState(Failed);
        QString msg = QString("读取固件文件失败: %1").arg(errorMsg);
        qDebug() << "[Scheduler][FirmwareUpgradeTask][onBinFileReadFinished]" << msg;
        LoggerManager::getInstance()->log(m_taskLogPath, Level::ERROR,
            QString("[FirmwareUpgradeTask][onBinFileReadFinished] %1").arg(msg).toStdString());
        emit finished(false, msg);
        return;
    }

    qDebug() << "[Scheduler][FirmwareUpgradeTask][onBinFileReadFinished] 固件文件读取成功，开始升级";
    LoggerManager::getInstance()->log(m_taskLogPath, Level::INFO,
        "[FirmwareUpgradeTask][onBinFileReadFinished] 固件文件读取成功，开始升级");

    startUpgrading();
}

void FirmwareUpgradeTask::startUpgrading()
{
    ModbusTcpMasterManager &manager = ModbusTcpMasterManager::instance();

    for (const QString &deviceId : m_deviceIds) {
        const QString logPath = QString("%1/%2").arg(QString::fromStdString(m_taskLogPath), deviceId);

        ModbusTcpMaster *master = manager.getMaster(deviceId);
        if (!master) {
            QString msg = QString("设备 %1 不存在").arg(deviceId);
            qDebug() << "[Scheduler][FirmwareUpgradeTask][startUpgrading]" << msg;
            LoggerManager::getInstance()->log(logPath.toStdString(), Level::WARN,
                QString("[FirmwareUpgradeTask][startUpgrading] %1").arg(msg).toStdString());
            LoggerManager::getInstance()->flush(logPath.toStdString());
            emit deviceFinished(deviceId, false, msg);
            continue;
        }

        FirmwareUpgrader *upgrader = master->firmwareUpgrader();
        if (!upgrader) {
            QString msg = QString("设备 %1 固件升级子模块不可用").arg(deviceId);
            qDebug() << "[Scheduler][FirmwareUpgradeTask][startUpgrading]" << msg;
            LoggerManager::getInstance()->log(logPath.toStdString(), Level::WARN,
                QString("[FirmwareUpgradeTask][startUpgrading] %1").arg(msg).toStdString());
            LoggerManager::getInstance()->flush(logPath.toStdString());
            emit deviceFinished(deviceId, false, msg);
            continue;
        }

        m_upgraderMap.insert(deviceId, upgrader);
        m_totalCount++;

        upgrader->setBinFileReader(m_binFileReader);

        connect(upgrader, &FirmwareUpgrader::stateChanged,
                this, &FirmwareUpgradeTask::onUpgraderStateChanged,
                Qt::QueuedConnection);
        connect(upgrader, &FirmwareUpgrader::progress,
                this, &FirmwareUpgradeTask::onUpgraderProgress,
                Qt::QueuedConnection);
        connect(upgrader, &FirmwareUpgrader::finished,
                this, &FirmwareUpgradeTask::onUpgraderFinished,
                Qt::QueuedConnection);

        // FirmwareUpgrader::start() 已内置线程分派，直接调用即可
        upgrader->start(m_binFilePath);

        qDebug() << "[Scheduler][FirmwareUpgradeTask][startUpgrading] 启动设备升级:" << deviceId << "信号已连接";
        LoggerManager::getInstance()->log(logPath.toStdString(), Level::INFO,
            QString("[FirmwareUpgradeTask][startUpgrading] 启动设备升级: %1 (信号已连接)").arg(deviceId).toStdString());
        LoggerManager::getInstance()->flush(logPath.toStdString());
    }

    if (m_totalCount == 0) {
        setState(Failed);
        qDebug() << "[Scheduler][FirmwareUpgradeTask][startUpgrading] 所有设备均无法启动固件升级";
        LoggerManager::getInstance()->log(m_taskLogPath, Level::ERROR,
            "[FirmwareUpgradeTask][startUpgrading] 所有设备均无法启动固件升级");
        emit finished(false, "所有设备均无法启动固件升级");
    } else {
        qDebug() << "[Scheduler][FirmwareUpgradeTask][startUpgrading] 共启动" << m_totalCount << "台设备升级";
        LoggerManager::getInstance()->log(m_taskLogPath, Level::INFO,
            QString("[FirmwareUpgradeTask][startUpgrading] 共启动 %1 台设备升级").arg(m_totalCount).toStdString());
    }
}

void FirmwareUpgradeTask::onUpgraderStateChanged(const QString &masterId,
                                                  FirmwareUpgrader::UpgradeState state,
                                                  const QString &logMessage,
                                                  const QByteArray &frame)
{
    const QString logPath = QString("%1/%2").arg(QString::fromStdString(m_taskLogPath), masterId);

    qDebug() << "[Scheduler][FirmwareUpgradeTask][onUpgraderStateChanged] 设备:" << masterId
             << "状态:" << static_cast<int>(state) << "消息:" << logMessage;
    LoggerManager::getInstance()->log(logPath.toStdString(), Level::INFO,
        QString("[FirmwareUpgradeTask][onUpgraderStateChanged] 设备=%1, 状态=%2, 消息=%3")
            .arg(masterId).arg(static_cast<int>(state)).arg(logMessage).toStdString());
    LoggerManager::getInstance()->flush(logPath.toStdString());

    emit deviceStateLog(masterId, state, logMessage, frame);
}

void FirmwareUpgradeTask::onUpgraderProgress(const QString &masterId, int percent)
{
    emit deviceProgress(masterId, percent);
}

void FirmwareUpgradeTask::onUpgraderFinished(const QString &masterId,
                                              bool success,
                                              FirmwareUpgrader::UpgradeState /*state*/,
                                              const QString &errorMessage)
{
    const QString logPath = QString("%1/%2").arg(QString::fromStdString(m_taskLogPath), masterId);

    if (m_stopped) return;

    qDebug() << "[Scheduler][FirmwareUpgradeTask][onUpgraderFinished] 接收设备完成信号:" << masterId
             << "成功:" << success << "错误:" << errorMessage;

    FirmwareUpgrader *upgrader = m_upgraderMap.value(masterId, nullptr);
    if (!upgrader) return;

    // 断开该 upgrader 的信号，避免重复处理
    disconnect(upgrader, nullptr, this, nullptr);

    QString resultMsg = success
        ? QString("[%1] 固件升级成功").arg(masterId)
        : QString("[%1] 固件升级失败: %2").arg(masterId, errorMessage);

    qDebug() << "[Scheduler][FirmwareUpgradeTask][onUpgraderFinished] 发送 deviceFinished 信号:" << resultMsg;
    LoggerManager::getInstance()->log(logPath.toStdString(),
        success ? Level::INFO : Level::ERROR,
        QString("[FirmwareUpgradeTask][onUpgraderFinished] %1").arg(resultMsg).toStdString());
    LoggerManager::getInstance()->flush(logPath.toStdString());

    emit deviceFinished(masterId, success, resultMsg);

    // 固件升级成功后，补发 QRCode 指令【import】
    if (success) {
        submitWriteQRCode(masterId);
    }

    m_finishedCount++;
    qDebug() << "[Scheduler][FirmwareUpgradeTask][onUpgraderFinished] 进度:" << m_finishedCount << "/" << m_totalCount;
    LoggerManager::getInstance()->log(logPath.toStdString(), Level::INFO,
        QString("[FirmwareUpgradeTask][onUpgraderFinished] 进度: %1/%2").arg(m_finishedCount).arg(m_totalCount).toStdString());
    LoggerManager::getInstance()->flush(logPath.toStdString());

    emit allProgress(m_finishedCount, m_totalCount);

    int overallPercent = (m_totalCount > 0) ? (m_finishedCount * 100 / m_totalCount) : 0;
    emit progress(overallPercent,
                  QString("已完成 %1/%2").arg(m_finishedCount).arg(m_totalCount));

    if (m_finishedCount >= m_totalCount) {
        setState(Finished);
        QString msg = QString("固件升级全部完成，共 %1 台设备").arg(m_totalCount);
        qDebug() << "[Scheduler][FirmwareUpgradeTask][onUpgraderFinished] 所有设备完成，发送 finished 信号";
        LoggerManager::getInstance()->log(m_taskLogPath, Level::INFO,
            QString("[FirmwareUpgradeTask][onUpgraderFinished] %1").arg(msg).toStdString());
        emit finished(true, msg);
    }
}

void FirmwareUpgradeTask::submitWriteQRCode(const QString &masterId)
{
    const QString logPath = QString("%1/%2").arg(QString::fromStdString(m_taskLogPath), masterId);

    ModbusTcpMasterManager &mgr = ModbusTcpMasterManager::instance();
    ModbusTcpMaster *master = mgr.getMaster(masterId);
    if (!master || !master->sender()) {
        qWarning() << "[Scheduler][FirmwareUpgradeTask] 下发 WriteQRCode 失败：master 不可用 masterId=" << masterId;
        LoggerManager::getInstance()->log(logPath.toStdString(), Level::WARN,
            QString("[FirmwareUpgradeTask][submitWriteQRCode] 下发 WriteQRCode 失败：master 不可用 masterId=%1").arg(masterId).toStdString());
        LoggerManager::getInstance()->flush(logPath.toStdString());
        return;
    }

    CommandPool *pool = mgr.commandPool();
    if (!pool || !pool->contains("WriteQRCode")) {
        qWarning() << "[Scheduler][FirmwareUpgradeTask] 下发 WriteQRCode 失败：指令池缺少 WriteQRCode";
        LoggerManager::getInstance()->log(logPath.toStdString(), Level::WARN,
            "[FirmwareUpgradeTask][submitWriteQRCode] 下发 WriteQRCode 失败：指令池缺少 WriteQRCode");
        LoggerManager::getInstance()->flush(logPath.toStdString());
        return;
    }

    ModbusCommand cmd = pool->clone("WriteQRCode");
    if (!cmd.isValid()) {
        qWarning() << "[Scheduler][FirmwareUpgradeTask] 下发 WriteQRCode 失败：指令克隆失败";
        LoggerManager::getInstance()->log(logPath.toStdString(), Level::WARN,
            "[FirmwareUpgradeTask][submitWriteQRCode] 下发 WriteQRCode 失败：指令克隆失败");
        LoggerManager::getInstance()->flush(logPath.toStdString());
        return;
    }

    cmd.module = CommandModule::BusinessCommandIssuer;

    // 将 qrcode 转换为 4 字节数据
    bool ok = false;
    quint32 qrcodeValue = masterId.toUInt(&ok);
    if (!ok) {
        qWarning() << "[Scheduler][FirmwareUpgradeTask] 下发 WriteQRCode 失败：qrcode 转换失败 masterId=" << masterId;
        LoggerManager::getInstance()->log(logPath.toStdString(), Level::WARN,
            QString("[FirmwareUpgradeTask][submitWriteQRCode] 下发 WriteQRCode 失败：qrcode 转换失败 masterId=%1").arg(masterId).toStdString());
        LoggerManager::getInstance()->flush(logPath.toStdString());
        return;
    }

    // 4 字节大端序：高字节在前
    QByteArray data;
    data.append(static_cast<char>((qrcodeValue >> 24) & 0xFF));
    data.append(static_cast<char>((qrcodeValue >> 16) & 0xFF));
    data.append(static_cast<char>((qrcodeValue >> 8) & 0xFF));
    data.append(static_cast<char>(qrcodeValue & 0xFF));

    // 更新请求寄存器数据
    cmd.request.registerValue = data;
    cmd.request.byteCount     = static_cast<quint8>(data.size());

    // 同步更新 rawBytes 中的数据段（FC 0x10，数据从偏移 7 开始，共 4 字节）
    if (cmd.request.functionCode == 0x10
        && cmd.request.rawBytes.size() >= 7 + 4
        && data.size() == 4) {
        for (int i = 0; i < 4; ++i)
            cmd.request.rawBytes[7 + i] = data[i];
    }

    // 记录待处理指令
    m_writeQRCodePendingMap[cmd.uuid] = masterId;

    qDebug() << "[Scheduler][FirmwareUpgradeTask] 下发 WriteQRCode masterId=" << masterId << "value=" << qrcodeValue << "uuid=" << cmd.uuid;
    LoggerManager::getInstance()->log(logPath.toStdString(), Level::INFO,
        QString("[FirmwareUpgradeTask][submitWriteQRCode] 下发 WriteQRCode masterId=%1 value=%2 uuid=%3").arg(masterId).arg(qrcodeValue).arg(cmd.uuid).toStdString());
    LoggerManager::getInstance()->flush(logPath.toStdString());

    if (SharedData::getOperationDispatchTask()) {
        SharedData::getOperationDispatchTask()->logMessage(
            QString("[WriteQRCode] Device %1 -> QRCode=%2 (固件升级后补发)").arg(masterId).arg(qrcodeValue));
    }

    // 连接信号监听响应
    ModbusCommandSender *sender = master->sender();
    auto conn = connect(sender, &ModbusCommandSender::commandFinished,
                        this, &FirmwareUpgradeTask::onWriteQRCodeFinished,
                        Qt::QueuedConnection);
    m_qrCodeConnections.append(conn);

    QMetaObject::invokeMethod(sender, [sender, cmd]() {
        sender->submit(cmd);
    }, Qt::QueuedConnection);
}

void FirmwareUpgradeTask::onWriteQRCodeFinished(ModbusCommand cmd, const QString &masterId)
{
    const QString logPath = QString("%1/%2").arg(QString::fromStdString(m_taskLogPath), masterId);

    if (m_stopped) return;

    // 检查是否是我们关注的 WriteQRCode 指令
    if (!m_writeQRCodePendingMap.contains(cmd.uuid)) return;

    m_writeQRCodePendingMap.remove(cmd.uuid);

    // 写入通讯日志
    {
        const QString sentTimeStr = cmd.sentMs > 0
            ? QDateTime::fromMSecsSinceEpoch(cmd.sentMs).toString(QStringLiteral("yyyy-MM-dd HH:mm:ss"))
            : QStringLiteral("-");
        int execStatus = 3;
        if (cmd.received)          execStatus = 0;
        else if (cmd.timedOut)     execStatus = 1;
        else if (cmd.sendCount > 1) execStatus = 2;
        const int retryCount = qMax(0, cmd.sendCount - 1);
        QString description;
        if (execStatus != 0) {
            description = cmd.errorMessage;
        } else {
            QVariantMap parsedData = CommandResponseParser::instance().parse(cmd);
            if (!parsedData.isEmpty()) {
                QStringList parts;
                for (auto it = parsedData.constBegin(); it != parsedData.constEnd(); ++it)
                    parts << QString("%1=%2").arg(it.key(), it.value().toString());
                description = parts.join(", ");
            }
        }
        if (description.isEmpty()) {
            description = QStringLiteral("OK");
        }
        if (LogDB::CommunicateLogDBCon *db = LogDB::DatabaseManager::instance().communicateLogCon()) {
            const QString respTimeStr = cmd.responseMs > 0
                ? QDateTime::fromMSecsSinceEpoch(cmd.responseMs).toString(QStringLiteral("yyyy-MM-dd HH:mm:ss"))
                : QString();
            db->insertRecord(sentTimeStr, respTimeStr, cmd.id, masterId,
                             execStatus, retryCount,
                             cmd.request.rawBytes, cmd.response.rawBytes, description,
                             UserPermission::Engineer);
        }
    }

    const bool ok = cmd.received && !cmd.timedOut && !cmd.checksumError && !cmd.deviceBusy;

    if (ok) {
        qDebug() << "[Scheduler][FirmwareUpgradeTask] WriteQRCode 指令成功 masterId=" << masterId << "uuid=" << cmd.uuid;
        LoggerManager::getInstance()->log(logPath.toStdString(), Level::INFO,
            QString("[FirmwareUpgradeTask][onWriteQRCodeFinished] WriteQRCode 指令成功 masterId=%1 uuid=%2").arg(masterId).arg(cmd.uuid).toStdString());
    } else {
        qWarning() << "[Scheduler][FirmwareUpgradeTask] WriteQRCode 指令失败 masterId=" << masterId
                   << "uuid=" << cmd.uuid << "received=" << cmd.received
                   << "timedOut=" << cmd.timedOut << "checksumError=" << cmd.checksumError
                   << "deviceBusy=" << cmd.deviceBusy;
        LoggerManager::getInstance()->log(logPath.toStdString(), Level::WARN,
            QString("[FirmwareUpgradeTask][onWriteQRCodeFinished] WriteQRCode 指令失败 masterId=%1 uuid=%2 received=%3 timedOut=%4 checksumError=%5 deviceBusy=%6")
                .arg(masterId).arg(cmd.uuid).arg(cmd.received).arg(cmd.timedOut).arg(cmd.checksumError).arg(cmd.deviceBusy).toStdString());
    }
    LoggerManager::getInstance()->flush(logPath.toStdString());
}
