#include "modbuscommand.h"
#include <QDebug>
#include <QDateTime>

// ============================================================
// ModbusCommand - Modbus 指令对象实现
// ============================================================

// 默认构造函数
ModbusCommand::ModbusCommand()
{
    // 默认初始化所有成员变量
}

// 拷贝构造函数
ModbusCommand::ModbusCommand(const ModbusCommand& other)
    : id(other.id),
      uuid(other.uuid),
      request(other.request),
      response(other.response),
      module(other.module),
      errorMessage(other.errorMessage),
      maxRetryCount(other.maxRetryCount),
      timeoutMs(other.timeoutMs),
      received(other.received),
      timedOut(other.timedOut),
      checksumError(other.checksumError),
      deviceBusy(other.deviceBusy),
      sendCount(other.sendCount),
      sentMs(other.sentMs),
      responseMs(other.responseMs)
{
    // 拷贝所有成员变量
}

// 拷贝赋值操作符
ModbusCommand& ModbusCommand::operator=(const ModbusCommand& other)
{
    if (this != &other) {
        id = other.id;
        uuid = other.uuid;
        request = other.request;
        response = other.response;
        module = other.module;
        errorMessage = other.errorMessage;
        maxRetryCount = other.maxRetryCount;
        timeoutMs = other.timeoutMs;
        received = other.received;
        timedOut = other.timedOut;
        checksumError = other.checksumError;
        deviceBusy = other.deviceBusy;
        sendCount = other.sendCount;
        sentMs = other.sentMs;
        responseMs = other.responseMs;
    }
    return *this;
}

QByteArray ModbusCommand::fromHexString(const QString& hexStr)
{
    QByteArray result;
    const QStringList tokens = hexStr.trimmed().split(' ', Qt::SkipEmptyParts);
    for (const QString& token : tokens) {
        bool ok = false;
        const uint byte = token.toUInt(&ok, 16);
        if (ok) {
            result.append(static_cast<char>(byte & 0xFF));
        } else {
            qDebug() << "ModbusCommand: 无效的十六进制字符串片段:" << token;
        }
    }
    return result;
}

QString ModbusCommand::toLogString() const
{
    const QString sentTimeStr = sentMs > 0
        ? QDateTime::fromMSecsSinceEpoch(sentMs).toString(QStringLiteral("yyyy-MM-dd HH:mm:ss.zzz"))
        : QStringLiteral("-");
    const QString respTimeStr = responseMs > 0
        ? QDateTime::fromMSecsSinceEpoch(responseMs).toString(QStringLiteral("yyyy-MM-dd HH:mm:ss.zzz"))
        : QString();
    
    int execStatus = 3;
    if (received)          execStatus = 0;
    else if (timedOut)     execStatus = 1;
    else if (sendCount > 1) execStatus = 2;
    
    QString statusStr;
    if (execStatus == 0) statusStr = "成功";
    else if (execStatus == 1) statusStr = "超时";
    else if (execStatus == 2) statusStr = "重试后失败";
    else statusStr = "未收到响应";
    
    const int retryCount = qMax(0, sendCount - 1);
    
    // 转换 rawBytes 为十六进制字符串
    auto bytesToHex = [](const QByteArray &bytes) -> QString {
        if (bytes.isEmpty()) return "无";
        QStringList hexList;
        for (int i = 0; i < bytes.size(); ++i) {
            hexList << QString("%1").arg(static_cast<quint8>(bytes[i]), 2, 16, QChar('0')).toUpper();
        }
        return hexList.join(" ");
    };

    // 转换 CRC 为十六进制字符串
    auto crcToHex = [](const QByteArray &crc) -> QString {
        if (crc.isEmpty() || crc.size() < 2) return "无";
        return QString("%1 %2")
            .arg(static_cast<quint8>(crc[0]), 2, 16, QChar('0')).toUpper()
            .arg(static_cast<quint8>(crc[1]), 2, 16, QChar('0')).toUpper();
    };

    // 转换 rawBytes + CRC 为十六进制字符串（包含校验码）
    auto bytesToHexWithCrc = [](const QByteArray &bytes, const QByteArray &crc) -> QString {
        QStringList hexList;
        if (!bytes.isEmpty()) {
            for (int i = 0; i < bytes.size(); ++i) {
                hexList << QString("%1").arg(static_cast<quint8>(bytes[i]), 2, 16, QChar('0')).toUpper();
            }
        }
        if (!crc.isEmpty() && crc.size() >= 2) {
            hexList << QString("%1").arg(static_cast<quint8>(crc[0]), 2, 16, QChar('0')).toUpper();
            hexList << QString("%1").arg(static_cast<quint8>(crc[1]), 2, 16, QChar('0')).toUpper();
        }
        return hexList.isEmpty() ? "无" : hexList.join(" ");
    };

    // 构建响应帧显示：失败时显示失败原因，但也显示响应帧（如果有）
    QString responseFrameStr;
    if (execStatus != 0) {
        // 发送失败，显示失败原因
        QStringList failureReasons;
        if (timedOut) failureReasons << "超时";
        if (checksumError) failureReasons << "校验错误";
        if (deviceBusy) failureReasons << "设备忙";
        if (!errorMessage.isEmpty()) failureReasons << errorMessage;
        QString failureStr = failureReasons.isEmpty() ? "失败" : failureReasons.join(", ");

        // 如果有响应数据，也显示响应帧（包含 CRC）
        if (!response.rawBytes.isEmpty()) {
            responseFrameStr = QString("%1, %2").arg(failureStr, bytesToHexWithCrc(response.rawBytes, response.crc));
        } else {
            responseFrameStr = failureStr;
        }
    } else {
        // 成功，显示响应帧（包含 CRC）
        responseFrameStr = bytesToHexWithCrc(response.rawBytes, response.crc);
    }

    // 计算使用时间
    qint64 elapsedTimeMs = 0;
    if (sentMs > 0 && responseMs > 0) {
        elapsedTimeMs = responseMs - sentMs;
    }
    const QString elapsedStr = (elapsedTimeMs > 0)
        ? QString("%1 ms").arg(elapsedTimeMs)
        : QStringLiteral("-");
    
    QString result;
    result += QString("  ID: %1\n").arg(id);
    result += QString("  发送时间: %1\n").arg(sentTimeStr);
    result += QString("  响应时间: %1\n").arg(respTimeStr);
    result += QString("  使用时间: %1\n").arg(elapsedStr);
    result += QString("  状态: %1\n").arg(statusStr);
    result += QString("  重发次数: %1\n").arg(retryCount);
    result += QString("  指令UUID: %1\n").arg(uuid);
    result += QString("  请求帧: %1\n").arg(bytesToHexWithCrc(request.rawBytes, request.crc));
    result += QString("  响应帧: %1").arg(responseFrameStr);
    
    return result;
}

