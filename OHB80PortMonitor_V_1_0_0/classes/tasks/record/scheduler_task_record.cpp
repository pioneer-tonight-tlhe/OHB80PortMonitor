#include "scheduler_task_record.h"

namespace {
// 统一时间格式，便于日志检索和人工阅读
QString formatDateTime(const QDateTime& time)
{
    if (!time.isValid()) {
        return QStringLiteral("-");
    }
    return time.toString(QStringLiteral("yyyy-MM-dd HH:mm:ss.zzz"));
}

// 耗时只在开始、结束时间都有效时展示，否则用 '-' 表示未知
QString formatElapsed(qint64 elapsedMs)
{
    if (elapsedMs < 0) {
        return QStringLiteral("-");
    }
    return QStringLiteral("%1 ms").arg(elapsedMs);
}

// 元信息为空时输出 '-'，避免日志中出现空字段
QString valueOrDash(const QString& value)
{
    return value.isEmpty() ? QStringLiteral("-") : value;
}
} // namespace

void SchedulerTaskRecord::reset()
{
    m_taskType.clear();
    m_deviceId.clear();
    m_status.clear();
    m_startTime = QDateTime();
    m_endTime = QDateTime();
    m_records.clear();
    m_subFunction.clear();
    m_commandId.clear();
    m_propertyName.clear();
    m_valueText.clear();
    m_errorMessage.clear();
    m_metadata.clear();
}

SchedulerTaskRecord& SchedulerTaskRecord::setTaskType(const QString& type)
{
    m_taskType = type;
    return *this;
}

SchedulerTaskRecord& SchedulerTaskRecord::setDeviceId(const QString& deviceId)
{
    m_deviceId = deviceId;
    return *this;
}

SchedulerTaskRecord& SchedulerTaskRecord::setStatus(const QString& status)
{
    m_status = status;
    return *this;
}

SchedulerTaskRecord& SchedulerTaskRecord::setSubFunction(const QString& subFunction)
{
    m_subFunction = subFunction;
    return *this;
}

SchedulerTaskRecord& SchedulerTaskRecord::setCommandId(const QString& commandId)
{
    m_commandId = commandId;
    return *this;
}

SchedulerTaskRecord& SchedulerTaskRecord::setPropertyName(const QString& propertyName)
{
    m_propertyName = propertyName;
    return *this;
}

SchedulerTaskRecord& SchedulerTaskRecord::setValueText(const QString& valueText)
{
    m_valueText = valueText;
    return *this;
}

SchedulerTaskRecord& SchedulerTaskRecord::setErrorMessage(const QString& errorMessage)
{
    m_errorMessage = errorMessage;
    return *this;
}

SchedulerTaskRecord& SchedulerTaskRecord::setMetadata(const QVariantMap& metadata)
{
    m_metadata = metadata;
    return *this;
}

SchedulerTaskRecord& SchedulerTaskRecord::addMetadata(const QString& key, const QVariant& value)
{
    m_metadata[key] = value;
    return *this;
}

void SchedulerTaskRecord::markTaskStart()
{
    m_startTime = QDateTime::currentDateTime();
}

void SchedulerTaskRecord::markTaskEnd()
{
    m_endTime = QDateTime::currentDateTime();
}

void SchedulerTaskRecord::setTaskStartTime(const QDateTime& time)
{
    m_startTime = time;
}

void SchedulerTaskRecord::setTaskEndTime(const QDateTime& time)
{
    m_endTime = time;
}

void SchedulerTaskRecord::appendRecord(const QString& record)
{
    m_records.append(record);
}

void SchedulerTaskRecord::appendRecords(const QStringList& records)
{
    m_records.append(records);
}

QStringList SchedulerTaskRecord::toLogLines() const
{
    QStringList lines;

    // 头部
    lines << QStringLiteral("============================= SchedulerTask 执行记录 =============================");
    lines << QStringLiteral("任务类型: %1").arg(valueOrDash(m_taskType));

    // 可选字段：子功能
    if (!m_subFunction.isEmpty()) {
        lines << QStringLiteral("子功能: %1").arg(m_subFunction);
    }

    lines << QStringLiteral("设备: %1").arg(valueOrDash(m_deviceId));

    // 可选字段：指令
    if (!m_commandId.isEmpty()) {
        lines << QStringLiteral("指令: %1").arg(m_commandId);
    }

    // 可选字段：属性
    if (!m_propertyName.isEmpty()) {
        lines << QStringLiteral("属性: %1").arg(m_propertyName);
    }

    // 可选字段：值
    if (!m_valueText.isEmpty()) {
        lines << QStringLiteral("值: %1").arg(m_valueText);
    }

    lines << QStringLiteral("状态: %1").arg(valueOrDash(m_status));
    lines << QStringLiteral("开始时间: %1").arg(formatDateTime(m_startTime));
    lines << QStringLiteral("结束时间: %1").arg(formatDateTime(m_endTime));
    lines << QStringLiteral("耗时: %1").arg(formatElapsed(elapsedMs()));

    // 可选字段：错误信息（只在有值时显示）
    if (!m_errorMessage.isEmpty()) {
        lines << QStringLiteral("错误信息: %1").arg(m_errorMessage);
    }

    lines << QStringLiteral("----------------------------- 执行明细 -----------------------------");

    if (m_records.isEmpty()) {
        lines << QStringLiteral("(无执行明细)");
    } else {
        // 保持任务执行时追加的顺序，便于按时间线排查问题
        lines.append(m_records);
    }

    // 可选字段：元数据（只在有内容时显示）
    if (!m_metadata.isEmpty()) {
        lines << QStringLiteral("----------------------------- 元数据 -----------------------------");
        for (auto it = m_metadata.constBegin(); it != m_metadata.constEnd(); ++it) {
            lines << QString("%1: %2").arg(it.key(), it.value().toString());
        }
    }

    lines << QStringLiteral("============================= SchedulerTask 执行记录结束 =============================");
    return lines;
}

QString SchedulerTaskRecord::toLogString() const
{
    // QStringList::join 不会在最后一行后追加分隔符，满足"最后一个字符串不加换行"
    return toLogLines().join(QStringLiteral("\n"));
}

bool SchedulerTaskRecord::isEmpty() const
{
    return m_records.isEmpty();
}

qint64 SchedulerTaskRecord::elapsedMs() const
{
    if (!m_startTime.isValid() || !m_endTime.isValid()) {
        return -1;
    }
    return m_startTime.msecsTo(m_endTime);
}
