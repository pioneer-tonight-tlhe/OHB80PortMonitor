#include "initialcommandissuer.h"

#include "idlepurgeconfig.h"
#include "modbuscommand/commandresponseparser.h"
#include "modbuslogger.h"
#include "modbustcpmaster.h"
#include "ohbdeviceconfig.h"

#include <QDebug>
#include <QSet>
#include <QVariantMap>
#include <QVector>
#include <QtGlobal>
#include <cmath>

// ============================================================
// InitialCommandIssuer - 初始指令下发器实现
// ============================================================

namespace {
constexpr const char* kWriteIdlePurgeEnableCommandId = "WriteIdlePurgeEnable";
constexpr const char* kWriteIdlePurgeTimeCommandId = "WriteIdlePurgeTime";
constexpr const char* kWriteIdlePurgeIntervalCommandId = "WriteIdlePurgeInterval";
constexpr const char* kSetBoardEnableCommandId = "SetBoardEnable";
constexpr const char* kWritePurgeFlowCommandId = "WritePurgeFlow";
constexpr const char* kReadVEFCFlowUnitAndMediumStatusCommandId = "ReadVEFCFlowUnitAndMediumStatus";
constexpr const char* kWritePneumaticValvePressureCommandId = "WritePneumaticValvePressure";
constexpr const char* kWriteUIRefreshTimeCommandId = "WriteUIRefreshTime";
constexpr const char* kWriteHumidityOffsetCommandId = "WriteHumidityOffset";
constexpr const char* kWriteHumidityOffsetThresholdCommandId = "WriteHumidityOffsetThreshold";
constexpr const char* kReadVersionCommandId = "ReadVersion";

constexpr int kPurgeFlowRegisterScale = 100;
constexpr int kVppePressureRegisterScale = 1000;
constexpr int kHumidityPercentRegisterScale = 100;

QString parseFirmwareVersion(const QByteArray& registerValue)
{
    if (registerValue.size() < 2) {
        return QString();
    }

    const quint8 majorByte = static_cast<quint8>(registerValue.at(0));
    const quint8 minorByte = static_cast<quint8>(registerValue.at(1));
    const bool majorIsDigit = majorByte >= '0' && majorByte <= '9';
    const bool minorIsDigit = minorByte >= '0' && minorByte <= '9';

    if (majorIsDigit && minorIsDigit) {
        return QString("%1.%2").arg(majorByte - '0').arg(minorByte - '0');
    }

    return QString("%1.%2").arg(majorByte).arg(minorByte);
}

QByteArray buildRegisterValue(quint16 value)
{
    QByteArray bytes(2, 0);
    bytes[0] = static_cast<char>((value >> 8) & 0xFF);
    bytes[1] = static_cast<char>(value & 0xFF);
    return bytes;
}

QByteArray buildRegisterPayload(const QVector<quint16>& values)
{
    QByteArray payload;
    payload.reserve(values.size() * 2);

    for (quint16 value : values) {
        payload.append(buildRegisterValue(value));
    }

    return payload;
}

quint16 toRegisterValue(double value, int scale)
{
    const double scaled = value * scale;
    const quint32 raw = static_cast<quint32>(std::round(scaled));
    return static_cast<quint16>(qBound<quint32>(0, raw, 0xFFFF));
}

void fillWriteSingleRegister(ModbusCommand& cmd, quint16 value)
{
    const QByteArray bytes = buildRegisterValue(value);

    cmd.request.registerValue = bytes;
    cmd.request.byteCount = static_cast<quint8>(bytes.size());
    if (cmd.request.functionCode == 0x06
        && cmd.request.rawBytes.size() >= 6
        && bytes.size() >= 2) {
        cmd.request.rawBytes[4] = bytes[0];
        cmd.request.rawBytes[5] = bytes[1];
    }

    cmd.response.registerValue = bytes;
    if (cmd.response.rawBytes.size() >= 6 && bytes.size() >= 2) {
        cmd.response.rawBytes[4] = bytes[0];
        cmd.response.rawBytes[5] = bytes[1];
    }
}

void fillWriteMultipleRegisters(ModbusCommand& cmd, const QByteArray& payload)
{
    cmd.request.registerValue = payload;
    cmd.request.byteCount = static_cast<quint8>(payload.size());

    constexpr int kPayloadOffset = 7;
    if (cmd.request.functionCode == 0x10
        && cmd.request.rawBytes.size() >= kPayloadOffset + payload.size()) {
        for (int i = 0; i < payload.size(); ++i) {
            cmd.request.rawBytes[kPayloadOffset + i] = payload[i];
        }
    }
}

QString commandResultText(const ModbusCommand& cmd)
{
    if (cmd.timedOut) {
        return QStringLiteral("timeout");
    }
    if (cmd.checksumError) {
        return QStringLiteral("checksum error");
    }
    if (cmd.deviceBusy) {
        return QStringLiteral("device busy");
    }
    if (!cmd.errorMessage.isEmpty()) {
        return cmd.errorMessage;
    }
    return QStringLiteral("not received");
}

QSet<QString> requiredInitialCommandIds()
{
    return {
        QString::fromLatin1(kWriteIdlePurgeEnableCommandId),
        QString::fromLatin1(kWriteIdlePurgeTimeCommandId),
        QString::fromLatin1(kWriteIdlePurgeIntervalCommandId),
        QString::fromLatin1(kSetBoardEnableCommandId),
        QString::fromLatin1(kWritePurgeFlowCommandId),
        QString::fromLatin1(kReadVEFCFlowUnitAndMediumStatusCommandId),
        QString::fromLatin1(kWritePneumaticValvePressureCommandId),
        QString::fromLatin1(kWriteUIRefreshTimeCommandId),
        QString::fromLatin1(kWriteHumidityOffsetCommandId),
        QString::fromLatin1(kWriteHumidityOffsetThresholdCommandId),
        QString::fromLatin1(kReadVersionCommandId)
    };
}
} // namespace

InitialCommandIssuer::InitialCommandIssuer(ModbusCommandSender& sender,
                                           const QString& masterId,
                                           ModbusTcpMaster* master,
                                           QObject* parent)
    : QObject(parent)
    , m_sender(sender)
    , m_masterId(masterId)
    , m_master(master)
{
    m_intervalTimer = new QTimer(this);
    m_intervalTimer->setSingleShot(true);
    connect(m_intervalTimer, &QTimer::timeout,
            this, &InitialCommandIssuer::sendCurrentCommand);
    connect(&m_sender, &ModbusCommandSender::commandFinished,
            this, &InitialCommandIssuer::onCommandFinished);
}

void InitialCommandIssuer::setCommandQueue(const QList<ModbusCommand>& queue)
{
    m_errorMsgList.clear();
    m_configuredCommandQueue = buildConfiguredCommandQueue(queue);
}

void InitialCommandIssuer::setInterval(int intervalMs)
{
    m_intervalMs = qMax(0, intervalMs);
}

void InitialCommandIssuer::setExecutionCount(int count)
{
    m_executionCount = qMax(1, count);
}

void InitialCommandIssuer::start()
{
    if (m_running) {
        return;
    }

    if (m_configuredCommandQueue.isEmpty()) {
        appendErrorMessage(QString("Initial command queue is empty: QRCode=%1").arg(m_masterId));
        emit finish(false, m_errorMsgList);
        deleteLater();
        return;
    }

    m_currentRoundQueue = m_configuredCommandQueue;
    m_nextRoundQueue.clear();
    m_finalFailedCommands.clear();
    m_pendingCommandMap.clear();
    m_currentIndex = 0;
    m_completedRounds = 0;
    m_running = true;
    m_stopRequested = false;

    qDebug() << "[InitialCommandIssuer] [设备ID=" << m_masterId << "] 启动初始化队列"
             << "指令数=" << m_currentRoundQueue.size()
             << "轮数=" << m_executionCount
             << "间隔=" << m_intervalMs << "ms";
    ModbusLogger::masterInfo(m_masterId, "ModbusTcpMaster", "InitialCommandIssuer", "start",
        QString("启动初始化队列，指令数=%1，轮数=%2，间隔=%3ms")
            .arg(m_currentRoundQueue.size())
            .arg(m_executionCount)
            .arg(m_intervalMs));

    sendCurrentCommand();
}

void InitialCommandIssuer::stop()
{
    if (!m_running && m_stopRequested) {
        return;
    }

    m_stopRequested = true;
    m_running = false;
    if (m_intervalTimer) {
        m_intervalTimer->stop();
    }
    m_pendingCommandMap.clear();
    qDebug() << "[InitialCommandIssuer] [设备ID=" << m_masterId << "] 初始化队列已停止";
    ModbusLogger::masterInfo(m_masterId, "ModbusTcpMaster", "InitialCommandIssuer", "stop",
        "初始化队列已停止");
}

QList<ModbusCommand> InitialCommandIssuer::buildConfiguredCommandQueue(const QList<ModbusCommand>& queue)
{
    QList<ModbusCommand> configuredQueue;
    configuredQueue.reserve(queue.size());

    const OHBDeviceConfigInfo deviceInfo = OHBDeviceConfig::getInstance().getDeviceByQRCode(m_masterId);
    if (deviceInfo.getQrCode().isEmpty()) {
        appendErrorMessage(QString("Initial command config missing: QRCode=%1").arg(m_masterId));
    }

    QSet<QString> actualIds;
    for (ModbusCommand cmd : queue) {
        actualIds.insert(cmd.id);
        configureCommand(cmd, deviceInfo);
        cmd.maxRetryCount = 0;
        configuredQueue.append(cmd);
    }

    const QSet<QString> requiredIds = requiredInitialCommandIds();
    for (const QString& requiredId : requiredIds) {
        if (!actualIds.contains(requiredId)) {
            appendErrorMessage(QString("Initial command missing in XML CommandSet: %1").arg(requiredId));
        }
    }

    return configuredQueue;
}

void InitialCommandIssuer::configureCommand(ModbusCommand& cmd, const OHBDeviceConfigInfo& deviceInfo)
{
    IdlePurgeConfig& idleConfig = IdlePurgeConfig::getInstance();
    cmd.module = CommandModule::InitialCommandIssuer;

    if (cmd.id == QLatin1String(kWriteIdlePurgeEnableCommandId)) {
        fillWriteSingleRegister(cmd, idleConfig.isEnabled() ? 1 : 0);
    } else if (cmd.id == QLatin1String(kWriteIdlePurgeTimeCommandId)) {
        fillWriteSingleRegister(cmd, static_cast<quint16>(qBound(0, idleConfig.getPurgeDurationSeconds(), 0xFFFF)));
    } else if (cmd.id == QLatin1String(kWriteIdlePurgeIntervalCommandId)) {
        fillWriteSingleRegister(cmd, static_cast<quint16>(qBound(0, idleConfig.getPurgeIntervalSeconds(), 0xFFFF)));
    } else if (cmd.id == QLatin1String(kSetBoardEnableCommandId)) {
        fillWriteSingleRegister(cmd, deviceInfo.isEnabled() ? 0 : 1);
    } else if (cmd.id == QLatin1String(kWritePurgeFlowCommandId)) {
        fillWriteSingleRegister(cmd, static_cast<quint16>(qBound(0,
                                                                 deviceInfo.getPurgeFlowLitersPerMinute() * kPurgeFlowRegisterScale,
                                                                 0xFFFF)));
    } else if (cmd.id == QLatin1String(kWritePneumaticValvePressureCommandId)) {
        fillWriteSingleRegister(cmd, toRegisterValue(deviceInfo.getVppePressureBar(), kVppePressureRegisterScale));
    } else if (cmd.id == QLatin1String(kWriteUIRefreshTimeCommandId)) {
        QVector<quint16> values;
        values.append(static_cast<quint16>(qBound(0, deviceInfo.getLogoTimeSeconds(), 0xFFFF)));
        values.append(static_cast<quint16>(qBound(0, deviceInfo.getPageTotalTimeSeconds(), 0xFFFF)));
        values.append(static_cast<quint16>(qBound(0, deviceInfo.getPageSwitchIntervalSeconds(), 0xFFFF)));
        const QByteArray payload = buildRegisterPayload(values);
        fillWriteMultipleRegisters(cmd, payload);
    } else if (cmd.id == QLatin1String(kWriteHumidityOffsetCommandId)) {
        fillWriteSingleRegister(cmd, toRegisterValue(deviceInfo.getHumidityOffsetPercent(), kHumidityPercentRegisterScale));
    } else if (cmd.id == QLatin1String(kWriteHumidityOffsetThresholdCommandId)) {
        fillWriteSingleRegister(cmd, toRegisterValue(deviceInfo.getHumidityLowerLimitPercent(), kHumidityPercentRegisterScale));
    }
}

void InitialCommandIssuer::sendCurrentCommand()
{
    if (!m_running || m_stopRequested) {
        return;
    }

    if (m_currentIndex >= m_currentRoundQueue.size()) {
        finishCurrentRoundIfNeeded();
        return;
    }

    ModbusCommand cmd = m_currentRoundQueue.at(m_currentIndex);
    cmd.resetState();
    cmd.maxRetryCount = 0;
    cmd.module = CommandModule::InitialCommandIssuer;

    const QString key = commandInstanceKey(cmd);
    m_pendingCommandMap.insert(key, cmd);

    qDebug() << "[InitialCommandIssuer] [设备ID=" << m_masterId << "] 下发初始化指令"
             << "轮次=" << (m_completedRounds + 1) << "/" << m_executionCount
             << "索引=" << m_currentIndex
             << "指令=" << cmd.id
             << "uuid=" << cmd.uuid;
    ModbusLogger::masterInfo(m_masterId, "ModbusTcpMaster", "InitialCommandIssuer", "sendCurrentCommand",
        QString("下发初始化指令，轮次=%1/%2，索引=%3，指令=%4，uuid=%5")
            .arg(m_completedRounds + 1)
            .arg(m_executionCount)
            .arg(m_currentIndex)
            .arg(cmd.id)
            .arg(cmd.uuid));

    ++m_currentIndex;
    m_sender.submit(cmd);
}

void InitialCommandIssuer::onCommandFinished(ModbusCommand cmd, QString masterId)
{
    if (!m_running || m_stopRequested) {
        return;
    }
    if (masterId != m_masterId) {
        return;
    }

    const QString key = commandInstanceKey(cmd);
    if (!m_pendingCommandMap.contains(key)) {
        return;
    }
    ModbusCommand originalCmd = m_pendingCommandMap.take(key);
    originalCmd.received = cmd.received;
    originalCmd.timedOut = cmd.timedOut;
    originalCmd.checksumError = cmd.checksumError;
    originalCmd.deviceBusy = cmd.deviceBusy;
    originalCmd.sendCount = cmd.sendCount;
    originalCmd.sentMs = cmd.sentMs;
    originalCmd.responseMs = cmd.responseMs;
    originalCmd.errorMessage = cmd.errorMessage;
    originalCmd.response = cmd.response;

    bool success = false;
    if (originalCmd.received) {
        success = handleCommandSuccess(originalCmd);
    }

    if (!success) {
        m_nextRoundQueue.append(originalCmd);
        qDebug() << "[InitialCommandIssuer] [设备ID=" << m_masterId << "] 初始化指令失败，进入下一轮候选"
                 << "指令=" << originalCmd.id
                 << "原因=" << commandResultText(originalCmd);
        ModbusLogger::masterWarn(m_masterId, "ModbusTcpMaster", "InitialCommandIssuer", "onCommandFinished",
            QString("初始化指令失败，进入下一轮候选，轮次=%1/%2，指令=%3，uuid=%4，原因=%5")
                .arg(m_completedRounds + 1)
                .arg(m_executionCount)
                .arg(originalCmd.id)
                .arg(originalCmd.uuid)
                .arg(commandResultText(originalCmd)));
    }

    if (m_currentIndex < m_currentRoundQueue.size()) {
        m_intervalTimer->start(m_intervalMs);
    } else {
        finishCurrentRoundIfNeeded();
    }
}

bool InitialCommandIssuer::handleCommandSuccess(ModbusCommand& cmd)
{
    if (!validateSuccessfulCommand(cmd)) {
        return false;
    }

    if (cmd.id != QLatin1String(kReadVersionCommandId)) {
        return true;
    }

    const QString version = parseFirmwareVersion(cmd.response.registerValue);
    if (version.isEmpty()) {
        ModbusLogger::masterWarn(m_masterId, "ModbusTcpMaster", "InitialCommandIssuer", "onCommandSucceeded",
            QString("ReadVersion 响应解析失败，响应寄存器字节数=%1")
                .arg(cmd.response.registerValue.size()));
        return false;
    }

    if (!m_master) {
        appendErrorMessage(QString("ReadVersion parsed but master is null, version=%1").arg(version));
        ModbusLogger::masterWarn(m_masterId, "ModbusTcpMaster", "InitialCommandIssuer", "onCommandSucceeded",
            QString("ReadVersion 解析成功但 Master 指针为空，版本号=%1").arg(version));
        return false;
    }

    m_master->m_firmwareVersion = version;

    qDebug() << "[InitialCommandIssuer] [设备ID=" << m_masterId << "] ReadVersion parsed:" << version;
    ModbusLogger::masterInfo(m_masterId, "ModbusTcpMaster", "InitialCommandIssuer", "onCommandSucceeded",
        QString("ReadVersion 解析完成，固件版本号=%1").arg(version));

    return true;
}

bool InitialCommandIssuer::validateSuccessfulCommand(ModbusCommand& cmd)
{
    if (cmd.id != QLatin1String(kReadVEFCFlowUnitAndMediumStatusCommandId)
        && cmd.id != QLatin1String(kReadVersionCommandId)) {
        return true;
    }

    if (cmd.id == QLatin1String(kReadVEFCFlowUnitAndMediumStatusCommandId)) {
        const QVariantMap parsedData = CommandResponseParser::instance().parse(cmd);
        if (parsedData.isEmpty()) {
            cmd.errorMessage = QString("ReadVEFCFlowUnitAndMediumStatus parse failed, register bytes=%1")
                                   .arg(cmd.response.registerValue.size());
            return false;
        }

        const bool unitOk = parsedData.value("unitOk").toBool();
        const bool mediumOk = parsedData.value("mediumOk").toBool();
        const int unitRaw = parsedData.value("unitRaw").toInt();
        const int mediumRaw = parsedData.value("mediumRaw").toInt();
        if (!unitOk || !mediumOk) {
            cmd.errorMessage = QString("ReadVEFCFlowUnitAndMediumStatus failed, unitRaw=%1, mediumRaw=%2")
                                   .arg(unitRaw)
                                   .arg(mediumRaw);
            return false;
        }
        return true;
    }

    const QString version = parseFirmwareVersion(cmd.response.registerValue);
    if (version.isEmpty()) {
        cmd.errorMessage = QString("ReadVersion parse failed, register bytes=%1")
                               .arg(cmd.response.registerValue.size());
        return false;
    }

    return true;
}

void InitialCommandIssuer::finishCurrentRoundIfNeeded()
{
    if (!m_running || m_stopRequested) {
        return;
    }
    if (m_currentIndex < m_currentRoundQueue.size() || !m_pendingCommandMap.isEmpty()) {
        return;
    }

    ++m_completedRounds;
    qDebug() << "[InitialCommandIssuer] [设备ID=" << m_masterId << "] 第" << m_completedRounds
             << "轮初始化完成，失败" << m_nextRoundQueue.size() << "条";
    ModbusLogger::masterInfo(m_masterId, "ModbusTcpMaster", "InitialCommandIssuer", "onRoundComplete",
        QString("第%1轮初始化完成，失败指令数=%2")
            .arg(m_completedRounds)
            .arg(m_nextRoundQueue.size()));

    if (m_nextRoundQueue.isEmpty()) {
        m_finalFailedCommands.clear();
        completeInitialization();
        return;
    }

    if (m_completedRounds >= m_executionCount) {
        m_finalFailedCommands = m_nextRoundQueue;
        completeInitialization();
        return;
    }

    startNextRound();
}

void InitialCommandIssuer::startNextRound()
{
    m_currentRoundQueue = m_nextRoundQueue;
    m_nextRoundQueue.clear();
    m_pendingCommandMap.clear();
    m_currentIndex = 0;

    qDebug() << "[InitialCommandIssuer] [设备ID=" << m_masterId << "] 启动下一轮初始化"
             << "轮次=" << (m_completedRounds + 1) << "/" << m_executionCount
             << "待重试指令数=" << m_currentRoundQueue.size();
    ModbusLogger::masterInfo(m_masterId, "ModbusTcpMaster", "InitialCommandIssuer", "startNextRound",
        QString("启动下一轮初始化，轮次=%1/%2，待重试指令数=%3")
            .arg(m_completedRounds + 1)
            .arg(m_executionCount)
            .arg(m_currentRoundQueue.size()));

    m_intervalTimer->start(m_intervalMs);
}

void InitialCommandIssuer::completeInitialization()
{
    for (const ModbusCommand& cmd : qAsConst(m_finalFailedCommands)) {
        appendErrorMessage(QString("Initial command failed: QRCode=%1, command=%2, reason=%3")
                               .arg(m_masterId)
                               .arg(cmd.id)
                               .arg(commandResultText(cmd)));
    }

    const bool isOk = m_finalFailedCommands.isEmpty() && m_errorMsgList.isEmpty();

    m_running = false;
    m_pendingCommandMap.clear();
    if (m_intervalTimer) {
        m_intervalTimer->stop();
    }

    qDebug() << "[InitialCommandIssuer] [设备ID=" << m_masterId << "] 初始化完成"
             << "isOk=" << isOk
             << "最终失败=" << m_finalFailedCommands.size()
             << "错误信息=" << m_errorMsgList.size();
    ModbusLogger::masterInfo(m_masterId, "ModbusTcpMaster", "InitialCommandIssuer", "completeInitialization",
        QString("初始化完成，isOk=%1，执行轮数=%2/%3，最终失败指令数=%4，错误信息数=%5")
            .arg(isOk)
            .arg(m_completedRounds)
            .arg(m_executionCount)
            .arg(m_finalFailedCommands.size())
            .arg(m_errorMsgList.size()));

    emit finish(isOk, m_errorMsgList);
    deleteLater();
}

void InitialCommandIssuer::appendErrorMessage(const QString& message)
{
    if (message.isEmpty() || m_errorMsgList.contains(message)) {
        return;
    }

    m_errorMsgList.append(message);
    ModbusLogger::masterWarn(m_masterId, "ModbusTcpMaster", "InitialCommandIssuer", "appendErrorMessage", message);
}

QString InitialCommandIssuer::commandInstanceKey(const ModbusCommand& cmd) const
{
    if (cmd.uuid != 0) {
        return QString("uuid:%1").arg(cmd.uuid);
    }

    return QString("id:%1").arg(cmd.id);
}
