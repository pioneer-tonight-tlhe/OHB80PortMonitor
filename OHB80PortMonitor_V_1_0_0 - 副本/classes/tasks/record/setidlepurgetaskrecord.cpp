#include "setidlepurgetaskrecord.h"

namespace {
// 统一时间格式，便于日志检索和人工阅读。
QString formatDateTime(const QDateTime& time)
{
    if (!time.isValid()) {
        return QStringLiteral("-");
    }
    return time.toString(QStringLiteral("yyyy-MM-dd HH:mm:ss.zzz"));
}

// 耗时只在开始、结束时间都有效时展示，否则用 '-' 表示未知。
QString formatElapsed(qint64 elapsedMs)
{
    if (elapsedMs < 0) {
        return QStringLiteral("-");
    }
    return QStringLiteral("%1 ms").arg(elapsedMs);
}

// 元信息为空时输出 '-'，避免日志中出现空字段。
QString valueOrDash(const QString& value)
{
    return value.isEmpty() ? QStringLiteral("-") : value;
}
} // namespace

void SetIdlePurgeTaskRecord::reset()
{
    m_subFunctionName.clear();
    m_commandId.clear();
    m_qrCode.clear();
    m_propertyName.clear();
    m_valueText.clear();
    m_resultText.clear();
    m_taskStartTime = QDateTime();
    m_taskEndTime = QDateTime();
    m_records.clear();
}

void SetIdlePurgeTaskRecord::setSubFunctionName(const QString& name)
{
    m_subFunctionName = name;
}

QString SetIdlePurgeTaskRecord::subFunctionName() const
{
    return m_subFunctionName;
}

void SetIdlePurgeTaskRecord::setCommandId(const QString& commandId)
{
    m_commandId = commandId;
}

QString SetIdlePurgeTaskRecord::commandId() const
{
    return m_commandId;
}

void SetIdlePurgeTaskRecord::setQrCode(const QString& qrCode)
{
    m_qrCode = qrCode;
}

QString SetIdlePurgeTaskRecord::qrCode() const
{
    return m_qrCode;
}

void SetIdlePurgeTaskRecord::setPropertyName(const QString& propertyName)
{
    m_propertyName = propertyName;
}

QString SetIdlePurgeTaskRecord::propertyName() const
{
    return m_propertyName;
}

void SetIdlePurgeTaskRecord::setValueText(const QString& valueText)
{
    m_valueText = valueText;
}

QString SetIdlePurgeTaskRecord::valueText() const
{
    return m_valueText;
}

void SetIdlePurgeTaskRecord::setResultText(const QString& resultText)
{
    m_resultText = resultText;
}

QString SetIdlePurgeTaskRecord::resultText() const
{
    return m_resultText;
}

void SetIdlePurgeTaskRecord::setTaskStartTime(const QDateTime& time)
{
    m_taskStartTime = time;
}

QDateTime SetIdlePurgeTaskRecord::taskStartTime() const
{
    return m_taskStartTime;
}

void SetIdlePurgeTaskRecord::setTaskEndTime(const QDateTime& time)
{
    m_taskEndTime = time;
}

QDateTime SetIdlePurgeTaskRecord::taskEndTime() const
{
    return m_taskEndTime;
}

void SetIdlePurgeTaskRecord::markTaskStart()
{
    m_taskStartTime = QDateTime::currentDateTime();
}

void SetIdlePurgeTaskRecord::markTaskEnd()
{
    m_taskEndTime = QDateTime::currentDateTime();
}

void SetIdlePurgeTaskRecord::appendRecord(const QString& record)
{
    m_records.append(record);
}

void SetIdlePurgeTaskRecord::appendRecords(const QStringList& records)
{
    m_records.append(records);
}

QStringList SetIdlePurgeTaskRecord::records() const
{
    return m_records;
}

QStringList SetIdlePurgeTaskRecord::toLogLines() const
{
    QStringList lines;

    // 头部记录固定元信息；执行过程中的细节只放在 m_records 中。
    lines << QStringLiteral("============================= SetIdlePurgeTask 执行记录 =============================");
    lines << QStringLiteral("任务名称: SetIdlePurgeTask");
    lines << QStringLiteral("子功能: %1").arg(valueOrDash(m_subFunctionName));
    lines << QStringLiteral("指令: %1").arg(valueOrDash(m_commandId));
    lines << QStringLiteral("设备: %1").arg(valueOrDash(m_qrCode));
    lines << QStringLiteral("属性: %1").arg(valueOrDash(m_propertyName));
    lines << QStringLiteral("值: %1").arg(valueOrDash(m_valueText));
    lines << QStringLiteral("结果: %1").arg(valueOrDash(m_resultText));
    lines << QStringLiteral("开始时间: %1").arg(formatDateTime(m_taskStartTime));
    lines << QStringLiteral("结束时间: %1").arg(formatDateTime(m_taskEndTime));
    lines << QStringLiteral("耗时: %1").arg(formatElapsed(elapsedMs()));
    lines << QStringLiteral("----------------------------- 执行明细 -----------------------------");

    if (m_records.isEmpty()) {
        lines << QStringLiteral("(无执行明细)");
    } else {
        // 保持任务执行时追加的顺序，便于按时间线排查问题。
        lines.append(m_records);
    }

    lines << QStringLiteral("============================= SetIdlePurgeTask 执行记录结束 =============================");
    return lines;
}

QString SetIdlePurgeTaskRecord::toLogString() const
{
    // QStringList::join 不会在最后一行后追加分隔符，满足“最后一个字符串不加换行”。
    return toLogLines().join(QStringLiteral("\n"));
}

bool SetIdlePurgeTaskRecord::isEmpty() const
{
    return m_records.isEmpty();
}

qint64 SetIdlePurgeTaskRecord::elapsedMs() const
{
    if (!m_taskStartTime.isValid() || !m_taskEndTime.isValid()) {
        return -1;
    }
    return m_taskStartTime.msecsTo(m_taskEndTime);
}
