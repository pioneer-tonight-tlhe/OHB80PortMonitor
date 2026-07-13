#ifndef PURGE_TASK_TYPES_H
#define PURGE_TASK_TYPES_H

#include <QJsonObject>
#include <QString>
#include <QVector>

enum class PurgeExecutionState {
    Pending,
    Running,
    Finished,
    Failed,
    Cancelled
};

struct PurgeActionDefinition {
    QString actionId;
    QString commandId;
    QJsonObject params;
    bool required = true;
};

struct PurgeStageDefinition {
    QString name;
    int durationSeconds = 0;
    QVector<PurgeActionDefinition> actions;
};

struct PurgeTaskDefinition {
    QString qrCode;
    int totalDurationSeconds = 0;
    QVector<PurgeStageDefinition> stages;

    bool isValid(QString *errorMessage = nullptr) const;
};

Q_DECLARE_METATYPE(PurgeExecutionState)
Q_DECLARE_METATYPE(PurgeActionDefinition)
Q_DECLARE_METATYPE(PurgeStageDefinition)
Q_DECLARE_METATYPE(PurgeTaskDefinition)

#endif // PURGE_TASK_TYPES_H
