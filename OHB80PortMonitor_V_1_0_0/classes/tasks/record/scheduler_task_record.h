#ifndef SCHEDULER_TASK_RECORD_H
#define SCHEDULER_TASK_RECORD_H

#include <QDateTime>
#include <QMetaType>
#include <QString>
#include <QStringList>
#include <QVariantMap>

// ====================================================================
// SchedulerTaskRecord —— 调度任务通用执行记录
//
// 设计目标：
//   1. 适用于所有调度层任务（SetIdlePurgeTask、SH85SelfCheckTask、UserManagementTask 等）
//   2. 任务执行过程中只写入内存 QStringList，不直接写 LoggerManager
//   3. 任务结束时通过 toLogString() 生成一条完整日志，减少日志队列入队次数
//   4. 本类只负责记录和格式化，不依赖 ILogger / LoggerManager
//   5. 支持可选字段，适应不同任务的需求
// ====================================================================
class SchedulerTaskRecord
{
public:
    SchedulerTaskRecord() = default;

    // 清空所有字段，便于对象复用
    void reset();

    // 必需字段的 setter（支持链式调用）
    SchedulerTaskRecord& setTaskType(const QString& type);
    SchedulerTaskRecord& setDeviceId(const QString& deviceId);
    SchedulerTaskRecord& setStatus(const QString& status);

    // 可选字段的 setter（支持链式调用）
    SchedulerTaskRecord& setSubFunction(const QString& subFunction);
    SchedulerTaskRecord& setCommandId(const QString& commandId);
    SchedulerTaskRecord& setPropertyName(const QString& propertyName);
    SchedulerTaskRecord& setValueText(const QString& valueText);
    SchedulerTaskRecord& setErrorMessage(const QString& errorMessage);

    // 元数据操作
    SchedulerTaskRecord& setMetadata(const QVariantMap& metadata);
    SchedulerTaskRecord& addMetadata(const QString& key, const QVariant& value);

    // 时间操作
    void markTaskStart();
    void markTaskEnd();
    void setTaskStartTime(const QDateTime& time);
    void setTaskEndTime(const QDateTime& time);

    // 记录操作
    void appendRecord(const QString& record);
    void appendRecords(const QStringList& records);

    // Getter 方法
    QString taskType() const { return m_taskType; }
    QString subFunction() const { return m_subFunction; }
    QString deviceId() const { return m_deviceId; }
    QString commandId() const { return m_commandId; }
    QString propertyName() const { return m_propertyName; }
    QString valueText() const { return m_valueText; }
    QString status() const { return m_status; }
    QString errorMessage() const { return m_errorMessage; }
    QDateTime taskStartTime() const { return m_startTime; }
    QDateTime taskEndTime() const { return m_endTime; }
    QStringList records() const { return m_records; }
    QVariantMap metadata() const { return m_metadata; }

    // 日志生成
    QStringList toLogLines() const;
    QString toLogString() const;

    // 工具方法
    bool isEmpty() const;
    qint64 elapsedMs() const;

private:
    // 必需字段
    QString m_taskType;        // 任务类型名称
    QString m_deviceId;        // 设备 ID/二维码
    QString m_status;          // 执行状态：成功/失败/跳过/取消
    QDateTime m_startTime;     // 任务开始时间
    QDateTime m_endTime;       // 任务结束时间
    QStringList m_records;     // 执行明细列表

    // 可选字段
    QString m_subFunction;     // 子功能/操作名称（可选）
    QString m_commandId;       // 指令 ID（可选）
    QString m_propertyName;    // 属性/参数名称（可选）
    QString m_valueText;       // 值文本表示（可选）
    QString m_errorMessage;    // 错误信息（可选）
    QVariantMap m_metadata;    // 自定义元数据
};

Q_DECLARE_METATYPE(SchedulerTaskRecord)

#endif // SCHEDULER_TASK_RECORD_H
