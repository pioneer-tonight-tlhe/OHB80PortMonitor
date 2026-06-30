#include "commandresponseparser.h"

#include <QDebug>

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
    registerParser("ReadFoupStatus", &CommandResponseParser::parseReadFoupStatus);
    registerParser("ReadIdlePurgeEnable", &CommandResponseParser::parseReadIdlePurgeEnable);
    registerParser("ReadIdlePurgeStatus", &CommandResponseParser::parseReadIdlePurgeStatus);
    registerParser("ReadIdlePurgeWorkingTime", &CommandResponseParser::parseReadIdlePurgeWorkingTime);
    registerParser("ReadIdlePurgeAll", &CommandResponseParser::parseReadIdlePurgeAll);
    registerParser("ReadVEFCPressure", &CommandResponseParser::parseReadVEFCPressure);
    registerParser("ReadVEFCTemperature", &CommandResponseParser::parseReadVEFCTemperature);
    registerParser("ReadVEFCFlowUnitAndMediumStatus",
                   &CommandResponseParser::parseReadVEFCFlowUnitAndMediumStatus);
    registerParser("ReadVersion", &CommandResponseParser::parseReadVersion);
    registerParser("ReadUIScreenVersion", &CommandResponseParser::parseReadUIScreenVersion);
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
    if (!cmd.received) {
        return {};
    }

    auto it = m_parsers.find(cmd.id);
    if (it == m_parsers.end()) {
        return {};
    }

    return it.value()(cmd);
}

static quint16 readU16BE(const QByteArray& payload, int offset)
{
    if (offset + 1 >= payload.size()) {
        return 0;
    }

    return (static_cast<quint8>(payload.at(offset)) << 8)
         | static_cast<quint8>(payload.at(offset + 1));
}

QVariantMap CommandResponseParser::parseReadFoupStatus(const ModbusCommand& cmd)
{
    QVariantMap result;
    const QByteArray& payload = cmd.response.registerValue;

    if (payload.size() < 18) {
        qWarning() << "[CommandResponseParser] ReadFoupStatus payload too short, actual="
                   << payload.size();
        return result;
    }

    result["inletPressure"] = readU16BE(payload, 0) / 100.0;

    const quint16 negativePressureRaw = readU16BE(payload, 2);
    const qint16 negativePressureSigned = static_cast<qint16>(negativePressureRaw);
    result["negativePressure"] = negativePressureSigned / 100.0;

    result["inletFlow"] = readU16BE(payload, 4) / 10.0;

    const quint16 humidityRaw = readU16BE(payload, 6);
    const quint16 temperatureRaw = readU16BE(payload, 8);

    result["foupIn"] = (readU16BE(payload, 10) != 0);

    const quint32 purgeHigh = readU16BE(payload, 12);
    const quint32 purgeLow = readU16BE(payload, 14);
    result["purgeTimeSec"] = (purgeHigh << 16) | purgeLow;

    const quint16 deviceStatus = readU16BE(payload, 16);
    result["vefcStatus"] = (deviceStatus & (1 << 0)) != 0;
    result["tempHumStatus"] = (deviceStatus & (1 << 1)) != 0;
    result["humidityReached"] = (deviceStatus & (1 << 2)) != 0;

    double humidityValue = humidityRaw / 100.0;
    double temperatureValue = temperatureRaw / 100.0;
    const bool tempHumSensorAbnormal = result.value("tempHumStatus").toBool();
    const bool invalidTempHumReading = (humidityRaw == 0xFFFF && temperatureRaw == 0xFFFF);
    if (tempHumSensorAbnormal && invalidTempHumReading) {
        humidityValue = 0.0;
        temperatureValue = 0.0;
    }

    result["humidity"] = humidityValue;
    result["temperature"] = temperatureValue;

    return result;
}

QVariantMap CommandResponseParser::parseReadIdlePurgeEnable(const ModbusCommand& cmd)
{
    QVariantMap result;
    const QByteArray& payload = cmd.response.registerValue;

    if (payload.size() < 2) {
        qWarning() << "[CommandResponseParser] ReadIdlePurgeEnable payload too short, actual="
                   << payload.size();
        return result;
    }

    result["idlePurgeEnabled"] = (readU16BE(payload, 0) != 0);
    return result;
}

QVariantMap CommandResponseParser::parseReadIdlePurgeStatus(const ModbusCommand& cmd)
{
    QVariantMap result;
    const QByteArray& payload = cmd.response.registerValue;

    if (payload.size() < 2) {
        qWarning() << "[CommandResponseParser] ReadIdlePurgeStatus payload too short, actual="
                   << payload.size();
        return result;
    }

    result["idleState"] = static_cast<int>(readU16BE(payload, 0));
    return result;
}

QVariantMap CommandResponseParser::parseReadIdlePurgeWorkingTime(const ModbusCommand& cmd)
{
    QVariantMap result;
    const QByteArray& payload = cmd.response.registerValue;

    if (payload.size() < 2) {
        qWarning() << "[CommandResponseParser] ReadIdlePurgeWorkingTime payload too short, actual="
                   << payload.size();
        return result;
    }

    result["idleWorkingTimeSec"] = static_cast<quint16>(readU16BE(payload, 0));
    return result;
}

QVariantMap CommandResponseParser::parseReadIdlePurgeAll(const ModbusCommand& cmd)
{
    QVariantMap result;
    const QByteArray& payload = cmd.response.registerValue;

    if (payload.size() < 6) {
        qWarning() << "[CommandResponseParser] ReadIdlePurgeAll payload too short, actual="
                   << payload.size();
        return result;
    }

    result["idlePurgeEnabled"] = (readU16BE(payload, 0) != 0);
    result["idleState"] = static_cast<int>(readU16BE(payload, 2));
    result["idleWorkingTimeSec"] = static_cast<quint16>(readU16BE(payload, 4));
    return result;
}

QVariantMap CommandResponseParser::parseReadVEFCPressure(const ModbusCommand& cmd)
{
    QVariantMap result;
    const QByteArray& payload = cmd.response.registerValue;

    if (payload.size() < 2) {
        qWarning() << "[CommandResponseParser] ReadVEFCPressure payload too short, actual="
                   << payload.size();
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
        qWarning() << "[CommandResponseParser] ReadVEFCTemperature payload too short, actual="
                   << payload.size();
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
        qWarning() << "[CommandResponseParser] ReadVEFCFlowUnitAndMediumStatus payload too short, actual="
                   << payload.size();
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
        qWarning() << "[CommandResponseParser] ReadVersion payload too short, actual="
                   << payload.size();
        return result;
    }

    const quint8 majorPart = static_cast<quint8>(payload.at(0));
    const quint8 minorPatchPart = static_cast<quint8>(payload.at(1));
    const quint8 minorPart = static_cast<quint8>((minorPatchPart >> 4) & 0x0F);
    const quint8 patchPart = static_cast<quint8>(minorPatchPart & 0x0F);

    result["firmwareVersionMajorRaw"] = majorPart;
    result["firmwareVersionMinorRaw"] = minorPart;
    result["firmwareVersionPatchRaw"] = patchPart;
    result["firmwareVersion"] = parseRegisterVersionString(payload, true);
    return result;
}

QVariantMap CommandResponseParser::parseReadUIScreenVersion(const ModbusCommand& cmd)
{
    QVariantMap result;
    const QByteArray& payload = cmd.response.registerValue;

    if (payload.size() < 2) {
        qWarning() << "[CommandResponseParser] ReadUIScreenVersion payload too short, actual="
                   << payload.size();
        return result;
    }

    const quint8 majorPart = static_cast<quint8>(payload.at(0));
    const quint8 minorPatchPart = static_cast<quint8>(payload.at(1));
    const quint8 minorPart = static_cast<quint8>((minorPatchPart >> 4) & 0x0F);
    const quint8 patchPart = static_cast<quint8>(minorPatchPart & 0x0F);

    result["uiScreenVersionMajor"] = majorPart;
    result["uiScreenVersionMinor"] = minorPart;
    result["uiScreenVersionPatch"] = patchPart;
    result["uiScreenVersion"] = parseRegisterVersionString(payload);
    return result;
}

QString CommandResponseParser::parseRegisterVersionString(const QByteArray& payload, bool withLeadingV)
{
    if (payload.size() < 2) {
        return QString();
    }

    const quint8 majorPart = static_cast<quint8>(payload.at(0));
    const quint8 minorPatchPart = static_cast<quint8>(payload.at(1));
    const quint8 minorPart = static_cast<quint8>((minorPatchPart >> 4) & 0x0F);
    const quint8 patchPart = static_cast<quint8>(minorPatchPart & 0x0F);

    return QString("%1%2.%3.%4")
        .arg(withLeadingV ? QStringLiteral("V") : QString())
        .arg(majorPart)
        .arg(minorPart)
        .arg(patchPart);
}
