#include "set_device_info_task.h"

#include "ohbdeviceconfig.h"
#include "app/shareddata.h"
#include "classes/foupofohbinfo.h"
#include "logdatabases/databasemanager.h"
#include "logdatabases/communicatelogdb/communicatelogdbcon.h"
#include "modbustcpmastermanager/modbuscommand/commandpool.h"
#include "modbustcpmastermanager/modbuscommand/commandresponseparser.h"
#include "modbustcpmastermanager/modbustcpmaster/modbustcpmaster.h"
#include "modbustcpmastermanager/modbustcpmaster/modbuscommandsender.h"
#include "modbustcpmastermanager/modbustcpmastermanager.h"
#include "scheduler/tasks/alarm_dispatch_task/alarm_dispatch_task.h"
#include "scheduler/tasks/operation_dispatch_task/operation_dispatch_task.h"

#include <QAbstractSocket>
#include <QDateTime>
#include <QDebug>
#include <QHostAddress>
#include <QStringList>
#include <QVariantMap>

SetDeviceInfoTask::SetDeviceInfoTask(QObject* parent)
    : SchedulerTask(parent)
{
}

void SetDeviceInfoTask::setDeviceInfo(const QString& oldQrCode,
                                      const QString& newQrCode,
                                      const QString& ip,
                                      quint16 port)
{
    m_oldQrCode = oldQrCode.trimmed();
    m_newQrCode = newQrCode.trimmed();
    m_ip = ip.trimmed();
    m_port = port;
}

void SetDeviceInfoTask::start()
{
    setState(Running);
    m_stopped = false;
    m_finished = false;
    m_pendingWriteQRCodeUuid = 0;
    m_writeQRCodeConnections.clear();
    m_oldIp.clear();
    m_oldPort = 0;

    QString errorMessage;
    if (!validateInput(&errorMessage)) {
        finishTask(false, errorMessage);
        return;
    }

    if (!prepareTask(&errorMessage)) {
        finishTask(false, errorMessage);
        return;
    }

    logMessage(QString("SetDeviceInfo task started: QRCode %1 -> %2, endpoint %3:%4")
                   .arg(m_oldQrCode, m_newQrCode, m_ip)
                   .arg(m_port));

    if (m_oldQrCode != m_newQrCode) {
        if (!submitWriteQRCodeCommand(&errorMessage)) {
            finishTask(false, errorMessage);
        }
        return;
    }

    applyDeviceInfoChange();
}

bool SetDeviceInfoTask::prepareTask(QString* errorMessage)
{
    OHBDeviceConfig& config = OHBDeviceConfig::getInstance();
    const QVector<OHBDeviceConfigInfo> devices = config.readDevices();
    bool oldFound = false;
    OHBDeviceConfigInfo oldInfo;

    for (const OHBDeviceConfigInfo& device : devices) {
        if (device.getQrCode() == m_oldQrCode) {
            oldFound = true;
            oldInfo = device;
        } else if (device.getQrCode() == m_newQrCode) {
            if (errorMessage) *errorMessage = QString("QRCode %1 already exists in config").arg(m_newQrCode);
            return false;
        }
    }

    if (!oldFound) {
        if (errorMessage) *errorMessage = QString("QRCode %1 does not exist in config").arg(m_oldQrCode);
        return false;
    }

    if (!SharedData::getFoupByQRCode(m_oldQrCode)) {
        if (errorMessage) *errorMessage = QString("QRCode %1 does not exist in shared data").arg(m_oldQrCode);
        return false;
    }

    ModbusTcpMasterManager& manager = ModbusTcpMasterManager::instance();
    if (!manager.getMaster(m_oldQrCode)) {
        if (errorMessage) *errorMessage = QString("Master %1 does not exist").arg(m_oldQrCode);
        return false;
    }

    m_oldIp = oldInfo.getIp();
    m_oldPort = oldInfo.getPort();
    if (errorMessage) errorMessage->clear();
    return true;
}

bool SetDeviceInfoTask::submitWriteQRCodeCommand(QString* errorMessage)
{
    if (m_stopped) {
        if (errorMessage) *errorMessage = QStringLiteral("SetDeviceInfoTask: cancelled");
        return false;
    }

    ModbusTcpMasterManager& manager = ModbusTcpMasterManager::instance();
    ModbusTcpMaster* master = manager.getMaster(m_oldQrCode);
    if (!master) {
        if (errorMessage) *errorMessage = QString("Master %1 does not exist").arg(m_oldQrCode);
        return false;
    }

    if (!master->isConnected()) {
        if (errorMessage) *errorMessage = QString("Master %1 is not connected, cannot write QRCode").arg(m_oldQrCode);
        return false;
    }

    ModbusCommandSender* sender = master->sender();
    if (!sender) {
        if (errorMessage) *errorMessage = QString("Master %1 sender is not available").arg(m_oldQrCode);
        return false;
    }

    CommandPool* pool = manager.commandPool();
    if (!pool || !pool->contains(QStringLiteral("WriteQRCode"))) {
        if (errorMessage) *errorMessage = QStringLiteral("CommandPool does not contain WriteQRCode");
        return false;
    }

    ModbusCommand cmd = pool->clone(QStringLiteral("WriteQRCode"));
    if (!cmd.isValid()) {
        if (errorMessage) *errorMessage = QStringLiteral("WriteQRCode command clone failed");
        return false;
    }

    if (cmd.request.functionCode != 0x10 || cmd.request.rawBytes.size() < 11) {
        if (errorMessage) *errorMessage = QStringLiteral("WriteQRCode command template is invalid");
        return false;
    }

    bool ok = false;
    const quint32 qrcodeValue = m_newQrCode.toUInt(&ok);
    if (!ok) {
        if (errorMessage) *errorMessage = QString("QRCode %1 cannot be converted to UInt").arg(m_newQrCode);
        return false;
    }

    QByteArray data;
    data.append(static_cast<char>((qrcodeValue >> 24) & 0xFF));
    data.append(static_cast<char>((qrcodeValue >> 16) & 0xFF));
    data.append(static_cast<char>((qrcodeValue >> 8) & 0xFF));
    data.append(static_cast<char>(qrcodeValue & 0xFF));

    cmd.module = CommandModule::BusinessCommandIssuer;
    cmd.request.registerValue = data;
    cmd.request.byteCount = static_cast<quint8>(data.size());
    cmd.request.rawBytes[6] = static_cast<char>(data.size());
    for (int i = 0; i < data.size(); ++i) {
        cmd.request.rawBytes[7 + i] = data.at(i);
    }

    m_pendingWriteQRCodeUuid = cmd.uuid;

    auto conn = connect(sender, &ModbusCommandSender::commandFinished,
                        this, &SetDeviceInfoTask::onWriteQRCodeFinished,
                        Qt::QueuedConnection);
    m_writeQRCodeConnections.append(conn);

    logMessage(QString("[WriteQRCode] Device %1 -> QRCode=%2 (SetDeviceInfo)")
                   .arg(m_oldQrCode, m_newQrCode));

    QMetaObject::invokeMethod(sender, [sender, cmd]() {
        sender->submit(cmd);
    }, Qt::QueuedConnection);

    if (errorMessage) errorMessage->clear();
    return true;
}

void SetDeviceInfoTask::onWriteQRCodeFinished(ModbusCommand cmd, const QString& masterId)
{
    Q_UNUSED(masterId)

    if (m_stopped || m_finished) return;
    if (cmd.uuid != m_pendingWriteQRCodeUuid) return;

    m_pendingWriteQRCodeUuid = 0;
    disconnectWriteQRCodeSignal();
    writeCommunicateLog(cmd);

    const bool ok = cmd.received
                 && !cmd.timedOut
                 && !cmd.checksumError
                 && !cmd.deviceBusy;

    if (!ok) {
        finishTask(false,
                   QString("SetDeviceInfo WriteQRCode failed: QRCode %1 -> %2, %3")
                       .arg(m_oldQrCode, m_newQrCode, writeQRCodeFailureReason(cmd)));
        return;
    }

    logMessage(QString("SetDeviceInfo WriteQRCode succeeded: QRCode %1 -> %2")
                   .arg(m_oldQrCode, m_newQrCode));

    applyDeviceInfoChange();
}

void SetDeviceInfoTask::applyDeviceInfoChange()
{
    if (m_stopped) {
        finishTask(false, QStringLiteral("SetDeviceInfoTask: cancelled"));
        return;
    }

    QString errorMessage;
    OHBDeviceConfig& config = OHBDeviceConfig::getInstance();
    ModbusTcpMasterManager& manager = ModbusTcpMasterManager::instance();

    if (!manager.reconfigureMaster(m_oldQrCode, m_newQrCode, m_ip, m_port, &errorMessage)) {
        finishTask(false, QString("SetDeviceInfo runtime reconfigure failed: %1").arg(errorMessage));
        return;
    }

    if (!config.updateDeviceInfoByQRCode(m_oldQrCode, m_newQrCode, m_ip, m_port)) {
        QString rollbackError;
        const bool rollbackOk = manager.reconfigureMaster(m_newQrCode,
                                                          m_oldQrCode,
                                                          m_oldIp,
                                                          m_oldPort,
                                                          &rollbackError);
        const QString rollbackText = rollbackOk
            ? QStringLiteral("runtime rollback succeeded")
            : QString("runtime rollback failed: %1").arg(rollbackError);
        finishTask(false, QString("SetDeviceInfo config update failed, %1").arg(rollbackText));
        return;
    }

    if (!SharedData::updateFoupDeviceInfoByQRCode(m_oldQrCode, m_newQrCode, m_ip, m_port)) {
        QString rollbackError;
        const bool rollbackOk = manager.reconfigureMaster(m_newQrCode,
                                                          m_oldQrCode,
                                                          m_oldIp,
                                                          m_oldPort,
                                                          &rollbackError);
        config.updateDeviceInfoByQRCode(m_newQrCode, m_oldQrCode, m_oldIp, m_oldPort);
        const QString rollbackText = rollbackOk
            ? QStringLiteral("runtime rollback succeeded")
            : QString("runtime rollback failed: %1").arg(rollbackError);
        finishTask(false, QString("SetDeviceInfo shared data update failed, %1").arg(rollbackText));
        return;
    }

    resolveOldQRCodeAlarms();

    finishTask(true, QString("SetDeviceInfo task completed: QRCode %1 -> %2, endpoint %3:%4")
                         .arg(m_oldQrCode, m_newQrCode, m_ip)
                         .arg(m_port));
}

void SetDeviceInfoTask::stop()
{
    if (m_finished) return;
    m_stopped = true;
    disconnectWriteQRCodeSignal();
    finishTask(false, QStringLiteral("SetDeviceInfoTask: cancelled"));
}

void SetDeviceInfoTask::resolveOldQRCodeAlarms()
{
    if (m_oldQrCode == m_newQrCode) {
        return;
    }

    AlarmDispatchTask* dispatcher = SharedData::getAlarmDispatchTask();
    if (!dispatcher) {
        logMessage(QString("SetDeviceInfo skipped alarm resolve for old QRCode %1: AlarmDispatchTask is null")
                       .arg(m_oldQrCode));
        return;
    }

    const int resolvedCount = dispatcher->submitResolveAllByQRCode(m_oldQrCode);
    logMessage(QString("SetDeviceInfo resolved %1 active alarms for old QRCode %2 after QRCode changed to %3")
                   .arg(resolvedCount)
                   .arg(m_oldQrCode, m_newQrCode));
}

void SetDeviceInfoTask::disconnectWriteQRCodeSignal()
{
    for (const QMetaObject::Connection& conn : m_writeQRCodeConnections) {
        QObject::disconnect(conn);
    }
    m_writeQRCodeConnections.clear();
}

void SetDeviceInfoTask::writeCommunicateLog(const ModbusCommand& cmd) const
{
    const QString sentTimeStr = cmd.sentMs > 0
        ? QDateTime::fromMSecsSinceEpoch(cmd.sentMs).toString(QStringLiteral("yyyy-MM-dd HH:mm:ss"))
        : QStringLiteral("-");

    int execStatus = 3;
    if (cmd.received) {
        execStatus = 0;
    } else if (cmd.timedOut) {
        execStatus = 1;
    } else if (cmd.sendCount > 1) {
        execStatus = 2;
    }

    const int retryCount = qMax(0, cmd.sendCount - 1);
    QString description;
    if (execStatus != 0) {
        description = cmd.errorMessage;
    } else {
        QVariantMap parsedData = CommandResponseParser::instance().parse(cmd);
        if (!parsedData.isEmpty()) {
            QStringList parts;
            for (auto it = parsedData.constBegin(); it != parsedData.constEnd(); ++it) {
                parts << QString("%1=%2").arg(it.key(), it.value().toString());
            }
            description = parts.join(QStringLiteral(", "));
        }
    }
    if (description.isEmpty()) {
        description = QStringLiteral("OK");
    }

    LogDB::CommunicateLogDBCon* db = LogDB::DatabaseManager::instance().communicateLogCon();
    if (!db) {
        return;
    }

    const QString respTimeStr = cmd.responseMs > 0
        ? QDateTime::fromMSecsSinceEpoch(cmd.responseMs).toString(QStringLiteral("yyyy-MM-dd HH:mm:ss"))
        : QString();

    QByteArray requestWithCrc = cmd.request.rawBytes;
    if (cmd.request.crc.size() >= 2) {
        requestWithCrc.append(cmd.request.crc);
    }

    QByteArray responseWithCrc = cmd.response.rawBytes;
    if (cmd.response.crc.size() >= 2) {
        responseWithCrc.append(cmd.response.crc);
    }

    db->insertRecord(sentTimeStr,
                     respTimeStr,
                     cmd.id,
                     m_oldQrCode,
                     execStatus,
                     retryCount,
                     requestWithCrc,
                     responseWithCrc,
                     description);
}

QString SetDeviceInfoTask::writeQRCodeFailureReason(const ModbusCommand& cmd) const
{
    QStringList reasons;
    if (!cmd.received) reasons << QStringLiteral("no response");
    if (cmd.timedOut) reasons << QStringLiteral("timed out");
    if (cmd.checksumError) reasons << QStringLiteral("checksum error");
    if (cmd.deviceBusy) reasons << QStringLiteral("device busy");
    if (!cmd.errorMessage.trimmed().isEmpty()) reasons << cmd.errorMessage.trimmed();
    if (reasons.isEmpty()) reasons << QStringLiteral("unknown error");
    return reasons.join(QStringLiteral(", "));
}

void SetDeviceInfoTask::finishTask(bool success, const QString& message)
{
    if (m_finished) return;
    m_finished = true;
    disconnectWriteQRCodeSignal();
    setState(success ? Finished : Failed);
    if (success) {
        logMessage(message);
    } else {
        logError(message);
    }
    emit finished(success, message);
}

void SetDeviceInfoTask::logMessage(const QString& message)
{
    if (auto* opTask = SharedData::getOperationDispatchTask()) {
        opTask->log(OperationDispatchTask::MsgType::Message, message, 0);
    }
}

void SetDeviceInfoTask::logError(const QString& message)
{
    if (auto* opTask = SharedData::getOperationDispatchTask()) {
        opTask->log(OperationDispatchTask::MsgType::Error, message, 0);
    }
}

bool SetDeviceInfoTask::validateInput(QString* errorMessage) const
{
    bool oldOk = false;
    bool newOk = false;
    m_oldQrCode.toInt(&oldOk);
    m_newQrCode.toInt(&newOk);
    if (!oldOk || !newOk) {
        if (errorMessage) *errorMessage = QStringLiteral("QRCode must be numeric");
        return false;
    }

    QHostAddress address;
    if (!address.setAddress(m_ip) || address.protocol() != QAbstractSocket::IPv4Protocol) {
        if (errorMessage) *errorMessage = QStringLiteral("IP must be a valid IPv4 address");
        return false;
    }

    if (m_port == 0) {
        if (errorMessage) *errorMessage = QStringLiteral("Port must be 1-65535");
        return false;
    }

    if (errorMessage) errorMessage->clear();
    return true;
}
