#include "network_status_task_qrcode_logger.h"

#include <QDateTime>

QRCodeWriteLogger::QRCodeWriteLogger(bool summaryEnable, bool devicesEnable)
    : m_summaryLogger("scheduler/network_status_task/qrcode/summary")
    , m_deviceLogger("scheduler/network_status_task/qrcode/devices")
    , m_deviceLogsEnabled(devicesEnable)
{
    m_summaryLogger.set_enable(summaryEnable);
    m_deviceLogger.set_enable(devicesEnable);
}

QString QRCodeWriteLogger::toHexSpacedUpper(const QByteArray& bytes)
{
    if (bytes.isEmpty()) return QStringLiteral("-");
    const QString hex = bytes.toHex().toUpper();
    QString out; out.reserve(hex.size() + hex.size() / 2);
    for (int i = 0; i < hex.size(); i += 2) {
        if (i > 0) out.append(' ');
        out.append(hex.mid(i, 2));
    }
    return out;
}

void QRCodeWriteLogger::logWriteQRCodeSuccess(const QString& deviceId, const ModbusCommand& cmd)
{
    const QString reqHex = toHexSpacedUpper(cmd.request.rawBytes);
    const QString respHex = toHexSpacedUpper(cmd.response.rawBytes);
    const QString sentTimeStr = cmd.sentMs > 0
        ? QDateTime::fromMSecsSinceEpoch(cmd.sentMs).toString(QStringLiteral("yyyy-MM-dd HH:mm:ss"))
        : QStringLiteral("-");
    const QString respTimeStr = cmd.responseMs > 0
        ? QDateTime::fromMSecsSinceEpoch(cmd.responseMs).toString(QStringLiteral("yyyy-MM-dd HH:mm:ss"))
        : QStringLiteral("-");
    const int retryCount = qMax(0, cmd.sendCount - 1);

    QString writeBackLine;
    if (cmd.request.registerValue.size() >= 4) {
        const QByteArray v = cmd.request.registerValue;
        quint32 qrv = (static_cast<quint8>(v[0]) << 24)
                    | (static_cast<quint8>(v[1]) << 16)
                    | (static_cast<quint8>(v[2]) << 8)
                    | (static_cast<quint8>(v[3]));
        const QString hex8 = QString("0x%1").arg(qrv, 8, 16, QChar('0')).toUpper();
        writeBackLine = QString("回写值: %1 (%2)").arg(qrv).arg(hex8);
    }

    QString qrReport;
    qrReport += QLatin1Char('\n');
    qrReport += QStringLiteral("指令: WriteQRCode\n");
    qrReport += QStringLiteral("请求帧: ") + reqHex + QLatin1Char('\n');
    qrReport += QStringLiteral("响应帧: ") + respHex + QLatin1Char('\n');
    qrReport += QStringLiteral("处理结果: 成功\n");
    if (!writeBackLine.isEmpty()) qrReport += writeBackLine + QLatin1Char('\n');
    qrReport += QStringLiteral("发送时间: ") + sentTimeStr + QLatin1Char('\n');
    qrReport += QStringLiteral("响应时间: ") + respTimeStr + QLatin1Char('\n');
    qrReport += QStringLiteral("重试次数: ") + QString::number(retryCount) + QLatin1Char('\n');

    deviceInfo(deviceId, QStringLiteral("指令完成"), qrReport);
}

void QRCodeWriteLogger::logWriteQRCodeFailure(const QString& deviceId, const ModbusCommand& cmd)
{
    const QString reqHex = toHexSpacedUpper(cmd.request.rawBytes);
    const QString respHex = toHexSpacedUpper(cmd.response.rawBytes);
    const QString sentTimeStr = cmd.sentMs > 0
        ? QDateTime::fromMSecsSinceEpoch(cmd.sentMs).toString(QStringLiteral("yyyy-MM-dd HH:mm:ss"))
        : QStringLiteral("-");
    const QString respTimeStr = cmd.responseMs > 0
        ? QDateTime::fromMSecsSinceEpoch(cmd.responseMs).toString(QStringLiteral("yyyy-MM-dd HH:mm:ss"))
        : QStringLiteral("-");
    const int retryCount = qMax(0, cmd.sendCount - 1);

    QString qrReport;
    qrReport += QLatin1Char('\n');
    qrReport += QStringLiteral("指令: WriteQRCode\n");
    qrReport += QStringLiteral("请求帧: ") + reqHex + QLatin1Char('\n');
    qrReport += QStringLiteral("响应帧: ") + respHex + QLatin1Char('\n');
    qrReport += QStringLiteral("处理结果: 失败\n");
    qrReport += QStringLiteral("失败原因: ") + (cmd.errorMessage.isEmpty() ? QStringLiteral("-") : cmd.errorMessage) + QLatin1Char('\n');
    qrReport += QStringLiteral("诊断:\n");
    qrReport += QStringLiteral("  received=") + QString(cmd.received ? "true" : "false") + QLatin1Char('\n');
    qrReport += QStringLiteral("  timedOut=") + QString(cmd.timedOut ? "true" : "false") + QLatin1Char('\n');
    qrReport += QStringLiteral("  checksumError=") + QString(cmd.checksumError ? "true" : "false") + QLatin1Char('\n');
    qrReport += QStringLiteral("  deviceBusy=") + QString(cmd.deviceBusy ? "true" : "false") + QLatin1Char('\n');
    qrReport += QStringLiteral("发送时间: ") + sentTimeStr + QLatin1Char('\n');
    qrReport += QStringLiteral("响应时间: ") + respTimeStr + QLatin1Char('\n');
    qrReport += QStringLiteral("重试次数: ") + QString::number(retryCount) + QLatin1Char('\n');

    deviceWarn(deviceId, QStringLiteral("指令完成"), qrReport);
}
ILogger& QRCodeWriteLogger::summaryLogger()
{
    return m_summaryLogger;
}

ILogger& QRCodeWriteLogger::deviceLogger(const QString& deviceId)
{
    Q_UNUSED(deviceId)
    return m_deviceLogger;
}

void QRCodeWriteLogger::setSummaryEnabled(bool enable)
{
    m_summaryLogger.set_enable(enable);
}

bool QRCodeWriteLogger::isSummaryEnabled() const
{
    return m_summaryLogger.get_enable();
}

void QRCodeWriteLogger::setDeviceLogsEnabled(bool enable)
{
    m_deviceLogsEnabled = enable;
    m_deviceLogger.set_enable(enable);
}

bool QRCodeWriteLogger::isDeviceLogsEnabled() const
{
    return m_deviceLogsEnabled;
}

void QRCodeWriteLogger::summaryInfo(const QString& action, const QString& message)
{
    summaryLogger().info(summaryMessage(action, message).toStdString());
}

void QRCodeWriteLogger::summaryWarn(const QString& action, const QString& message)
{
    summaryLogger().warn(summaryMessage(action, message).toStdString());
}

void QRCodeWriteLogger::summaryError(const QString& action, const QString& message)
{
    summaryLogger().error(summaryMessage(action, message).toStdString());
}

void QRCodeWriteLogger::deviceInfo(const QString& deviceId, const QString& action, const QString& message)
{
    deviceLogger(deviceId).info(deviceMessage(deviceId, action, message).toStdString());
}

void QRCodeWriteLogger::deviceWarn(const QString& deviceId, const QString& action, const QString& message)
{
    deviceLogger(deviceId).warn(deviceMessage(deviceId, action, message).toStdString());
}

void QRCodeWriteLogger::deviceError(const QString& deviceId, const QString& action, const QString& message)
{
    deviceLogger(deviceId).error(deviceMessage(deviceId, action, message).toStdString());
}

QString QRCodeWriteLogger::normalizedDeviceId(const QString& deviceId)
{
    const QString trimmed = deviceId.trimmed();
    return trimmed.isEmpty() ? QStringLiteral("unknown") : trimmed;
}

QString QRCodeWriteLogger::summaryMessage(const QString& action, const QString& message)
{
    return QString("[scheduler][NetworkStatusTask][QRCode][Summary][%1]：%2").arg(action, message);
}

QString QRCodeWriteLogger::deviceMessage(const QString& deviceId, const QString& action, const QString& message)
{
    return QString("[scheduler][NetworkStatusTask][QRCode][Device][%1]：设备ID=%2 %3")
        .arg(action, normalizedDeviceId(deviceId), message);
}
