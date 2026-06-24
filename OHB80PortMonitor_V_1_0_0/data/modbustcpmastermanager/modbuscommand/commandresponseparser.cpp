#include "commandresponseparser.h"
#include <QDebug>

// ============================================================
// CommandResponseParser 实现
// ============================================================

CommandResponseParser& CommandResponseParser::instance()
{
    static CommandResponseParser s_instance;
    return s_instance;
}

CommandResponseParser::CommandResponseParser()
{
    registerBuiltinParsers();
}

void CommandResponseParser::registerBuiltinParsers()
{
    registerParser("ReadFoupStatus",          &CommandResponseParser::parseReadFoupStatus);
    registerParser("ReadIdlePurgeEnable",     &CommandResponseParser::parseReadIdlePurgeEnable);
    registerParser("ReadIdlePurgeStatus",     &CommandResponseParser::parseReadIdlePurgeStatus);
    registerParser("ReadIdlePurgeWorkingTime",&CommandResponseParser::parseReadIdlePurgeWorkingTime);
    registerParser("ReadIdlePurgeAll",        &CommandResponseParser::parseReadIdlePurgeAll);
    registerParser("ReadVEFCPressure",        &CommandResponseParser::parseReadVEFCPressure);
    registerParser("ReadVEFCTemperature",     &CommandResponseParser::parseReadVEFCTemperature);
    registerParser("ReadVEFCFlowUnitAndMediumStatus",
                   &CommandResponseParser::parseReadVEFCFlowUnitAndMediumStatus);
    registerParser("ReadVersion",             &CommandResponseParser::parseReadVersion);
    registerParser("ReadUIScreenVersion",     &CommandResponseParser::parseReadUIScreenVersion);
}

void CommandResponseParser::registerParser(const QString& commandId, ParseFunc func)
{
    m_parsers.insert(commandId, func);
}

bool CommandResponseParser::hasParser(const QString& commandId) const
{
    return m_parsers.contains(commandId);
}

QVariantMap CommandResponseParser::parse(const ModbusCommand& cmd) const
{
    if (!cmd.received) return {};

    auto it = m_parsers.find(cmd.id);
    if (it == m_parsers.end()) return {};

    return it.value()(cmd);
}

// ── 辅助函数 ─────────────────────────────────────────────────

static quint16 readU16BE(const QByteArray& payload, int offset)
{
    if (offset + 1 >= payload.size()) return 0;
    return (static_cast<quint8>(payload.at(offset)) << 8)
         | static_cast<quint8>(payload.at(offset + 1));
}

// ── 各指令解析实现 ────────────────────────────────────────────

QVariantMap CommandResponseParser::parseReadFoupStatus(const ModbusCommand& cmd)
{
    QVariantMap result;
    const QByteArray& payload = cmd.response.registerValue;

    // 9个寄存器 × 2字节 = 18字节
    if (payload.size() < 18) {
        qWarning() << "[CommandResponseParser] ReadFoupStatus 响应字节数不足，实际=" << payload.size();
        return result;
    }

    // CH_1 (bytes 0-1): 进气主路气压值 = raw / 100，单位 Mpa
    result["inletPressure"]     = readU16BE(payload, 0) / 100.0;

    // CH_2 (bytes 2-3): 主路负压值（补码，需要解析为有符号数）
    quint16 negativePressureRaw = readU16BE(payload, 2);
    int16_t negativePressureSigned = static_cast<int16_t>(negativePressureRaw);  // 补码转为有符号数
    double negativePressureKpa = negativePressureSigned / 100.0;  // -10~10 Kpa
    result["negativePressure"]  = negativePressureKpa;  // 单位 Kpa

    // CH_3 (bytes 4-5): 进气主路流量值 (4~200 L/Min) = raw / 10
    result["inletFlow"]         = readU16BE(payload, 4) / 10.0;

    // CH_4 (bytes 6-7): 真空回路相对湿度 (0~100%) = raw / 100
    result["humidity"]          = readU16BE(payload, 6) / 100.0;

    // CH_5 (bytes 8-9): 真空回路气体温度 (0~100℃) = raw / 100
    result["temperature"]       = readU16BE(payload, 8) / 100.0;

    // CH_6 (bytes 10-11): FOUP 在位状态 (1=in, 0=out)
    result["foupIn"]            = (readU16BE(payload, 10) != 0);

    // CH_7 (bytes 12-13): FOUP IN 充气秒数高16位
    quint32 purgeHigh = readU16BE(payload, 12);
    // CH_8 (bytes 14-15): FOUP IN 充气秒数低16位
    quint32 purgeLow  = readU16BE(payload, 14);
    result["purgeTimeSec"]      = (purgeHigh << 16) | purgeLow;

    // CH_9 (bytes 16-17): 设备状态（2byte）
    quint16 deviceStatus = readU16BE(payload, 16);
    result["vefcStatus"]        = (deviceStatus & (1 << 0)) != 0;  // bit0: 0=OK, 1=NG
    result["tempHumStatus"]     = (deviceStatus & (1 << 1)) != 0;  // bit1: 0=OK, 1=NG
    result["humidityReached"]   = (deviceStatus & (1 << 2)) != 0;  // bit2: 0=OK, 1=NG

    return result;
}

QVariantMap CommandResponseParser::parseReadIdlePurgeEnable(const ModbusCommand& cmd)
{
    QVariantMap result;
    const QByteArray& payload = cmd.response.registerValue;

    // 1个寄存器 × 2字节 = 2字节
    if (payload.size() < 2) {
        qWarning() << "[CommandResponseParser] ReadIdlePurgeEnable 响应字节数不足，实际=" << payload.size();
        return result;
    }

    // CH_1: 0=关闭，1=开启
    result["idlePurgeEnabled"] = (readU16BE(payload, 0) != 0);
    return result;
}

QVariantMap CommandResponseParser::parseReadIdlePurgeStatus(const ModbusCommand& cmd)
{
    QVariantMap result;
    const QByteArray& payload = cmd.response.registerValue;

    // 1个寄存器 × 2字节 = 2字节
    if (payload.size() < 2) {
        qWarning() << "[CommandResponseParser] ReadIdlePurgeStatus 响应字节数不足，实际=" << payload.size();
        return result;
    }

    // CH_1: 0=空闲, 1=准备阶段(10S), 2=充气, 3=充气(间隔)
    result["idleState"] = static_cast<int>(readU16BE(payload, 0));
    return result;
}

QVariantMap CommandResponseParser::parseReadIdlePurgeWorkingTime(const ModbusCommand& cmd)
{
    QVariantMap result;
    const QByteArray& payload = cmd.response.registerValue;

    // 1个寄存器 × 2字节 = 2字节
    if (payload.size() < 2) {
        qWarning() << "[CommandResponseParser] ReadIdlePurgeWorkingTime 响应字节数不足，实际=" << payload.size();
        return result;
    }

    // CH_1: 计时时长（秒）
    result["idleWorkingTimeSec"] = static_cast<quint16>(readU16BE(payload, 0));
    return result;
}

QVariantMap CommandResponseParser::parseReadIdlePurgeAll(const ModbusCommand& cmd)
{
    QVariantMap result;
    const QByteArray& payload = cmd.response.registerValue;

    // 3个寄存器 × 2字节 = 6字节
    if (payload.size() < 6) {
        qWarning() << "[CommandResponseParser] ReadIdlePurgeAll 响应字节数不足，实际=" << payload.size();
        return result;
    }

    // CH_1 (0x001D): Idle Purge 使能 (0=关闭, 1=开启)
    result["idlePurgeEnabled"]  = (readU16BE(payload, 0) != 0);
    // CH_2 (0x001E): Idle Purge 状态 (0=Idle, 1=准备, 2=充气, 3=充气间隔)
    result["idleState"]         = static_cast<int>(readU16BE(payload, 2));
    // CH_3 (0x001F): 工作计时时长（秒）
    result["idleWorkingTimeSec"] = static_cast<quint16>(readU16BE(payload, 4));
    return result;
}

QVariantMap CommandResponseParser::parseReadVEFCPressure(const ModbusCommand& cmd)
{
    QVariantMap result;
    const QByteArray& payload = cmd.response.registerValue;

    if (payload.size() < 2) {
        qWarning() << "[CommandResponseParser] ReadVEFCPressure 响应字节数不足，实际=" << payload.size();
        return result;
    }

    result["sensorPressure"] = readU16BE(payload, 0) / 10.0;
    return result;
}

QVariantMap CommandResponseParser::parseReadVEFCTemperature(const ModbusCommand& cmd)
{
    QVariantMap result;
    const QByteArray& payload = cmd.response.registerValue;

    if (payload.size() < 2) {
        qWarning() << "[CommandResponseParser] ReadVEFCTemperature 响应字节数不足，实际=" << payload.size();
        return result;
    }

    result["sensorTemperature"] = readU16BE(payload, 0) / 100.0;
    return result;
}

QVariantMap CommandResponseParser::parseReadVEFCFlowUnitAndMediumStatus(const ModbusCommand& cmd)
{
    QVariantMap result;
    const QByteArray& payload = cmd.response.registerValue;

    if (payload.size() < 2) {
        qWarning() << "[CommandResponseParser] ReadVEFCFlowUnitAndMediumStatus 响应字节数不足，实际=" << payload.size();
        return result;
    }

    const quint8 unitRaw = static_cast<quint8>(payload.at(0));
    const quint8 mediumRaw = static_cast<quint8>(payload.at(1));

    result["unitRaw"] = unitRaw;
    result["mediumRaw"] = mediumRaw;
    result["unitOk"] = (unitRaw == 0);
    result["mediumOk"] = (mediumRaw == 0);
    return result;
}

QVariantMap CommandResponseParser::parseReadVersion(const ModbusCommand& cmd)
{
    QVariantMap result;
    const QByteArray& payload = cmd.response.registerValue;

    if (payload.size() < 2) {
        qWarning() << "[CommandResponseParser] ReadVersion 响应字节数不足，实际=" << payload.size();
        return result;
    }

    const quint8 majorByte = static_cast<quint8>(payload.at(0));
    const quint8 minorByte = static_cast<quint8>(payload.at(1));
    const bool majorIsDigit = majorByte >= '0' && majorByte <= '9';
    const bool minorIsDigit = minorByte >= '0' && minorByte <= '9';

    QString version;
    if (majorIsDigit && minorIsDigit) {
        version = QString("%1.%2").arg(majorByte - '0').arg(minorByte - '0');
    } else {
        version = QString("%1.%2").arg(majorByte).arg(minorByte);
    }

    result["firmwareVersionMajorRaw"] = majorByte;
    result["firmwareVersionMinorRaw"] = minorByte;
    result["firmwareVersion"] = version;
    return result;
}

QVariantMap CommandResponseParser::parseReadUIScreenVersion(const ModbusCommand& cmd)
{
    QVariantMap result;
    const QByteArray& payload = cmd.response.registerValue;

    if (payload.size() < 2) {
        qWarning() << "[CommandResponseParser] ReadUIScreenVersion 响应字节数不足，实际=" << payload.size();
        return result;
    }

    const quint8 majorPart = static_cast<quint8>(payload.at(0));
    const quint8 minorPatchPart = static_cast<quint8>(payload.at(1));
    const quint8 minorPart = static_cast<quint8>((minorPatchPart >> 4) & 0x0F);
    const quint8 patchPart = static_cast<quint8>(minorPatchPart & 0x0F);

    result["uiScreenVersionMajor"] = majorPart;
    result["uiScreenVersionMinor"] = minorPart;
    result["uiScreenVersionPatch"] = patchPart;
    result["uiScreenVersion"] = QString("%1.%2.%3")
                                    .arg(majorPart)
                                    .arg(minorPart)
                                    .arg(patchPart);
    return result;
}
