#ifndef SH85SELFCHECKTASKRECORD_H
#define SH85SELFCHECKTASKRECORD_H

#include <QDateTime>
#include <QMetaType>
#include <QString>
#include <QStringList>

// ====================================================================
// SH85SelfCheckTaskRecord —— 单台设备的一次 SH85 自检执行记录
//
// 设计目标：
//   1. 自检过程中只追加内存明细，不直接写日志。
//   2. 单台设备结束时生成一条完整日志，降低日志队列入队次数。
//   3. 本类只负责记录和格式化，不依赖 ILogger / LoggerManager。
// ====================================================================
class SH85SelfCheckTaskRecord
{
public:
    SH85SelfCheckTaskRecord() = default;

    void reset();

    void setModeText(const QString& modeText);
    QString modeText() const;

    void setRoundId(const QString& roundId);
    QString roundId() const;

    void setQrCode(const QString& qrCode);
    QString qrCode() const;

    void setResultText(const QString& resultText);
    QString resultText() const;

    void setResultCode(const QString& resultCode);
    QString resultCode() const;

    void setDescription(const QString& description);
    QString description() const;

    void setTaskStartTime(const QDateTime& time);
    QDateTime taskStartTime() const;

    void setTaskEndTime(const QDateTime& time);
    QDateTime taskEndTime() const;

    void markTaskStart();
    void markTaskEnd();

    void appendRecord(const QString& record);
    void appendRecords(const QStringList& records);
    QStringList records() const;

    QStringList toLogLines() const;
    QString toLogString() const;

    bool isEmpty() const;
    qint64 elapsedMs() const;

private:
    QString m_modeText;
    QString m_roundId;
    QString m_qrCode;
    QString m_resultText;
    QString m_resultCode;
    QString m_description;
    QDateTime m_taskStartTime;
    QDateTime m_taskEndTime;
    QStringList m_records;
};

Q_DECLARE_METATYPE(SH85SelfCheckTaskRecord)

#endif // SH85SELFCHECKTASKRECORD_H
