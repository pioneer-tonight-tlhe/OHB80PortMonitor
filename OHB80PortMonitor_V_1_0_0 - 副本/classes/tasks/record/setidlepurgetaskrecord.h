#ifndef SETIDLEPURGETASKRECORD_H
#define SETIDLEPURGETASKRECORD_H

#include <QDateTime>
#include <QMetaType>
#include <QString>
#include <QStringList>

// ====================================================================
// SetIdlePurgeTaskRecord —— SetIdlePurgeTask 的单次执行记录
//
// 设计目标：
//   1. 任务执行过程中只写入内存 QStringList，不直接写 LoggerManager。
//   2. 任务结束时通过 toLogString() 生成一条完整日志，减少日志队列入队次数。
//   3. 本类只负责记录和格式化，不依赖 ILogger / LoggerManager。
//   4. 一台设备对应一个记录对象，最终由任务统一批量写入日志。
// ====================================================================
class SetIdlePurgeTaskRecord
{
public:
    SetIdlePurgeTaskRecord() = default;

    // 清空子功能、指令、时间和所有执行明细，便于对象复用。
    void reset();

    // 子功能名称，例如 set_idle_purge_enable / set_purge_duration。
    void setSubFunctionName(const QString& name);
    QString subFunctionName() const;

    // 本次任务使用的 Modbus 指令 ID，例如 WriteIdlePurgeEnable。
    void setCommandId(const QString& commandId);
    QString commandId() const;

    // 设备二维码 / masterId。
    void setQrCode(const QString& qrCode);
    QString qrCode() const;

    // 被设置的属性名称，例如 Idle Purge Enable。
    void setPropertyName(const QString& propertyName);
    QString propertyName() const;

    // 被设置的属性值，例如 enable / 60 s。
    void setValueText(const QString& valueText);
    QString valueText() const;

    // 设备执行结果，例如 成功 / 失败 / 取消。
    void setResultText(const QString& resultText);
    QString resultText() const;

    // 任务开始时间。外部可传入指定时间，或调用 markTaskStart() 自动记录当前时间。
    void setTaskStartTime(const QDateTime& time);
    QDateTime taskStartTime() const;

    // 任务结束时间。外部可传入指定时间，或调用 markTaskEnd() 自动记录当前时间。
    void setTaskEndTime(const QDateTime& time);
    QDateTime taskEndTime() const;

    // 以当前系统时间标记任务开始。
    void markTaskStart();

    // 以当前系统时间标记任务结束。
    void markTaskEnd();

    // 追加一条执行明细。调用方负责组织单条明细的内容。
    void appendRecord(const QString& record);

    // 批量追加执行明细，保持传入顺序。
    void appendRecords(const QStringList& records);

    // 返回原始执行明细列表，不包含任务头和任务尾。
    QStringList records() const;

    // 返回完整日志行列表，包含任务头、元信息、执行明细和结束标记。
    QStringList toLogLines() const;

    // 返回完整日志文本。行之间用 '\n' 拼接，最后一行后不追加额外换行。
    QString toLogString() const;

    // 是否没有任何执行明细。
    bool isEmpty() const;

    // 任务耗时，单位 ms。开始或结束时间无效时返回 -1。
    qint64 elapsedMs() const;

private:
    QString m_subFunctionName;   // 子功能名称
    QString m_commandId;         // Modbus 指令 ID
    QString m_qrCode;            // 设备二维码 / masterId
    QString m_propertyName;      // 被设置的属性名称
    QString m_valueText;         // 被设置的属性值
    QString m_resultText;        // 执行结果
    QDateTime m_taskStartTime;   // 任务开始时间
    QDateTime m_taskEndTime;     // 任务结束时间
    QStringList m_records;       // 执行明细列表，每个元素是一条日志片段
};

Q_DECLARE_METATYPE(SetIdlePurgeTaskRecord)

#endif // SETIDLEPURGETASKRECORD_H
