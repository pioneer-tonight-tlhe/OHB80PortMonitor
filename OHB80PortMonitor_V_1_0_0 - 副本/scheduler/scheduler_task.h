#ifndef SCHEDULER_TASK_H
#define SCHEDULER_TASK_H

#include <QObject>
#include <QMetaType>
#include <QString>
#include <QVariantMap>
#include <QUuid>

class SchedulerTask : public QObject
{
    Q_OBJECT

public:
    enum State { Pending, Running, Paused, Finished, Failed, Cancelled };
    Q_ENUM(State)

    enum Priority { Low, Normal, High, Urgent };
    Q_ENUM(Priority)

    explicit SchedulerTask(QObject *parent = nullptr)
        : QObject(parent)
        , m_taskId(QUuid::createUuid().toString(QUuid::WithoutBraces))
        , m_state(Pending)
        , m_priority(Normal)
    {}

    virtual ~SchedulerTask() = default;

    // 任务生命周期（子类必须实现）
    Q_INVOKABLE virtual void start() = 0;
    Q_INVOKABLE virtual void stop() = 0;

    // 可选重写
    virtual void pause() {}
    virtual void resume() {}

    // 任务属性
    QString taskId() const { return m_taskId; }
    virtual QString taskType() const = 0;
    State state() const { return m_state; }
    Priority priority() const { return m_priority; }
    void setPriority(Priority p) { m_priority = p; }

    // 是否是长驻任务（长驻任务不占用并发槽位，不会被自动清理）
    virtual bool isPersistent() const { return false; }

    // 是否是周期任务
    virtual bool isRecurring() const { return false; }
    virtual int intervalMs() const { return 0; }

signals:
    void stateChanged(SchedulerTask::State newState);
    void progress(int percent, const QString &msg);
    void finished(bool success, const QString &msg);
    void dataResult(const QString &key, const QVariantMap &data);

protected:
    void setState(State s) {
        if (m_state != s) {
            m_state = s;
            emit stateChanged(m_state);
        }
    }

private:
    QString m_taskId;
    State m_state;
    Priority m_priority;
};

Q_DECLARE_METATYPE(SchedulerTask::State)

#endif // SCHEDULER_TASK_H
