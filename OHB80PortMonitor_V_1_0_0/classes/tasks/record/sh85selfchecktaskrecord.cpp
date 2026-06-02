#include "sh85selfchecktaskrecord.h"

namespace {
QString formatDateTime(const QDateTime& time)
{
    return time.isValid()
        ? time.toString(QStringLiteral("yyyy-MM-dd HH:mm:ss.zzz"))
        : QStringLiteral("-");
}

QString formatElapsed(qint64 elapsedMs)
{
    return elapsedMs >= 0 ? QStringLiteral("%1 ms").arg(elapsedMs) : QStringLiteral("-");
}

QString valueOrDash(const QString& value)
{
    return value.isEmpty() ? QStringLiteral("-") : value;
}
} // namespace

void SH85SelfCheckTaskRecord::reset()
{
    m_modeText.clear();
    m_roundId.clear();
    m_qrCode.clear();
    m_resultText.clear();
    m_resultCode.clear();
    m_description.clear();
    m_taskStartTime = QDateTime();
    m_taskEndTime = QDateTime();
    m_records.clear();
}

void SH85SelfCheckTaskRecord::setModeText(const QString& modeText)
{
    m_modeText = modeText;
}

QString SH85SelfCheckTaskRecord::modeText() const
{
    return m_modeText;
}

void SH85SelfCheckTaskRecord::setRoundId(const QString& roundId)
{
    m_roundId = roundId;
}

QString SH85SelfCheckTaskRecord::roundId() const
{
    return m_roundId;
}

void SH85SelfCheckTaskRecord::setQrCode(const QString& qrCode)
{
    m_qrCode = qrCode;
}

QString SH85SelfCheckTaskRecord::qrCode() const
{
    return m_qrCode;
}

void SH85SelfCheckTaskRecord::setResultText(const QString& resultText)
{
    m_resultText = resultText;
}

QString SH85SelfCheckTaskRecord::resultText() const
{
    return m_resultText;
}

void SH85SelfCheckTaskRecord::setResultCode(const QString& resultCode)
{
    m_resultCode = resultCode;
}

QString SH85SelfCheckTaskRecord::resultCode() const
{
    return m_resultCode;
}

void SH85SelfCheckTaskRecord::setDescription(const QString& description)
{
    m_description = description;
}

QString SH85SelfCheckTaskRecord::description() const
{
    return m_description;
}

void SH85SelfCheckTaskRecord::setTaskStartTime(const QDateTime& time)
{
    m_taskStartTime = time;
}

QDateTime SH85SelfCheckTaskRecord::taskStartTime() const
{
    return m_taskStartTime;
}

void SH85SelfCheckTaskRecord::setTaskEndTime(const QDateTime& time)
{
    m_taskEndTime = time;
}

QDateTime SH85SelfCheckTaskRecord::taskEndTime() const
{
    return m_taskEndTime;
}

void SH85SelfCheckTaskRecord::markTaskStart()
{
    m_taskStartTime = QDateTime::currentDateTime();
}

void SH85SelfCheckTaskRecord::markTaskEnd()
{
    m_taskEndTime = QDateTime::currentDateTime();
}

void SH85SelfCheckTaskRecord::appendRecord(const QString& record)
{
    m_records.append(record);
}

void SH85SelfCheckTaskRecord::appendRecords(const QStringList& records)
{
    m_records.append(records);
}

QStringList SH85SelfCheckTaskRecord::records() const
{
    return m_records;
}

QStringList SH85SelfCheckTaskRecord::toLogLines() const
{
    QStringList lines;
    lines << QStringLiteral("============================= SH85SelfCheck 执行记录 =============================");
    lines << QStringLiteral("任务名称: SH85SelfCheck");
    lines << QStringLiteral("模式: %1").arg(valueOrDash(m_modeText));
    if (!m_roundId.isEmpty()) {
        lines << QStringLiteral("轮次ID: %1").arg(m_roundId);
    }
    lines << QStringLiteral("设备: %1").arg(valueOrDash(m_qrCode));
    lines << QStringLiteral("结果: %1").arg(valueOrDash(m_resultText));
    lines << QStringLiteral("结果码: %1").arg(valueOrDash(m_resultCode));
    lines << QStringLiteral("说明: %1").arg(valueOrDash(m_description));
    lines << QStringLiteral("开始时间: %1").arg(formatDateTime(m_taskStartTime));
    lines << QStringLiteral("结束时间: %1").arg(formatDateTime(m_taskEndTime));
    lines << QStringLiteral("耗时: %1").arg(formatElapsed(elapsedMs()));
    lines << QStringLiteral("----------------------------- 执行明细 -----------------------------");
    lines << (m_records.isEmpty() ? QStringLiteral("无") : m_records.join(QStringLiteral("\n")));
    lines << QStringLiteral("============================= SH85SelfCheck 执行记录结束 =============================");
    return lines;
}

QString SH85SelfCheckTaskRecord::toLogString() const
{
    return toLogLines().join(QStringLiteral("\n"));
}

bool SH85SelfCheckTaskRecord::isEmpty() const
{
    return m_records.isEmpty();
}

qint64 SH85SelfCheckTaskRecord::elapsedMs() const
{
    if (!m_taskStartTime.isValid() || !m_taskEndTime.isValid()) {
        return -1;
    }
    return m_taskStartTime.msecsTo(m_taskEndTime);
}
