#include "purgetaskconfig.h"

#include "appconfig.h"

#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QSettings>

namespace {
const char *const TaskGroup = "Task";
const char *const DefaultQRCodeKey = "DefaultQRCode";
const char *const TotalDurationSecondsKey = "TotalDuration_s";
const char *const StageCountKey = "StageCount";
const char *const StageNameKey = "Name";
const char *const StageDurationSecondsKey = "Duration_s";
const char *const ActionCountKey = "ActionCount";
const char *const DefaultQRCode = "12001";

QJsonObject parseJsonObject(const QString &jsonText)
{
    QJsonParseError error;
    const QJsonDocument doc = QJsonDocument::fromJson(jsonText.toUtf8(), &error);
    if (error.error != QJsonParseError::NoError || !doc.isObject()) {
        return {};
    }
    return doc.object();
}

QString actionKey(int actionIndex, const QString &suffix)
{
    return QStringLiteral("Action%1_%2").arg(actionIndex).arg(suffix);
}
}

PurgeTaskConfig& PurgeTaskConfig::getInstance()
{
    static PurgeTaskConfig instance;
    return instance;
}

PurgeTaskConfig::PurgeTaskConfig()
{
    const QString configDir = AppConfig::getInstance().getConfigDir();
    QDir dir(configDir);
    if (!dir.exists()) {
        dir.mkpath(QStringLiteral("."));
    }

    m_configFilePath = dir.filePath(QStringLiteral("purge_task.ini"));
    ensureDefaultConfigFile();
}

QString PurgeTaskConfig::readDefaultQRCode() const
{
    ensureDefaultConfigFile();

    QSettings settings(m_configFilePath, QSettings::IniFormat);
    settings.beginGroup(TaskGroup);
    const QString qrCode = settings.value(DefaultQRCodeKey, DefaultQRCode).toString().trimmed();
    settings.endGroup();
    return qrCode.isEmpty() ? QString::fromLatin1(DefaultQRCode) : qrCode;
}

PurgeTaskDefinition PurgeTaskConfig::readTaskDefinition() const
{
    return readTaskDefinition(readDefaultQRCode());
}

PurgeTaskDefinition PurgeTaskConfig::readTaskDefinition(const QString &qrCode) const
{
    ensureDefaultConfigFile();

    QSettings settings(m_configFilePath, QSettings::IniFormat);
    PurgeTaskDefinition definition;

    settings.beginGroup(TaskGroup);
    definition.qrCode = qrCode.trimmed().isEmpty()
        ? settings.value(DefaultQRCodeKey, DefaultQRCode).toString().trimmed()
        : qrCode.trimmed();
    definition.totalDurationSeconds = settings.value(TotalDurationSecondsKey, 600).toInt();
    const int stageCount = settings.value(StageCountKey, 0).toInt();
    settings.endGroup();

    for (int stageIndex = 1; stageIndex <= stageCount; ++stageIndex) {
        const QString stageGroup = QStringLiteral("Stage%1").arg(stageIndex);
        settings.beginGroup(stageGroup);

        PurgeStageDefinition stage;
        stage.name = settings.value(StageNameKey,
                                    QStringLiteral("Stage %1").arg(stageIndex)).toString();
        stage.durationSeconds = settings.value(StageDurationSecondsKey, 0).toInt();
        const int actionCount = settings.value(ActionCountKey, 0).toInt();

        for (int actionIndex = 1; actionIndex <= actionCount; ++actionIndex) {
            PurgeActionDefinition action;
            action.actionId = QStringLiteral("stage_%1_action_%2")
                                  .arg(stageIndex)
                                  .arg(actionIndex);
            action.commandId = settings.value(actionKey(actionIndex, QStringLiteral("CommandId"))).toString().trimmed();
            action.params = parseJsonObject(settings.value(actionKey(actionIndex, QStringLiteral("Params"))).toString());
            action.required = settings.value(actionKey(actionIndex, QStringLiteral("Required")), true).toBool();
            stage.actions.append(action);
        }

        settings.endGroup();
        definition.stages.append(stage);
    }

    QString errorMessage;
    if (!definition.isValid(&errorMessage)) {
        return defaultDefinition(qrCode);
    }

    return definition;
}

QString PurgeTaskConfig::getConfigPath() const
{
    return m_configFilePath;
}

void PurgeTaskConfig::ensureDefaultConfigFile() const
{
    if (!QFile::exists(m_configFilePath)) {
        writeDefaultConfigFile();
    }
}

void PurgeTaskConfig::writeDefaultConfigFile() const
{
    QSettings settings(m_configFilePath, QSettings::IniFormat);

    settings.beginGroup(TaskGroup);
    settings.setValue(DefaultQRCodeKey, DefaultQRCode);
    settings.setValue(TotalDurationSecondsKey, 600);
    settings.setValue(StageCountKey, 2);
    settings.endGroup();

    settings.beginGroup(QStringLiteral("Stage1"));
    settings.setValue(StageNameKey, QStringLiteral("Stage 1"));
    settings.setValue(StageDurationSecondsKey, 30);
    settings.setValue(ActionCountKey, 1);
    settings.setValue(QStringLiteral("Action1_CommandId"), QStringLiteral("WritePurgeFlow"));
    settings.setValue(QStringLiteral("Action1_Params"), QStringLiteral("{\"flow_l_min\":10}"));
    settings.setValue(QStringLiteral("Action1_Required"), true);
    settings.endGroup();

    settings.beginGroup(QStringLiteral("Stage2"));
    settings.setValue(StageNameKey, QStringLiteral("Stage 2"));
    settings.setValue(StageDurationSecondsKey, 570);
    settings.setValue(ActionCountKey, 1);
    settings.setValue(QStringLiteral("Action1_CommandId"), QStringLiteral("WritePurgeFlow"));
    settings.setValue(QStringLiteral("Action1_Params"), QStringLiteral("{\"flow_l_min\":30}"));
    settings.setValue(QStringLiteral("Action1_Required"), true);
    settings.endGroup();

    settings.sync();
}

PurgeTaskDefinition PurgeTaskConfig::defaultDefinition(const QString &qrCode) const
{
    PurgeTaskDefinition definition;
    definition.qrCode = qrCode.trimmed().isEmpty()
        ? QString::fromLatin1(DefaultQRCode)
        : qrCode.trimmed();
    definition.totalDurationSeconds = 600;

    PurgeStageDefinition stage1;
    stage1.name = QStringLiteral("Stage 1");
    stage1.durationSeconds = 30;
    PurgeActionDefinition action1;
    action1.actionId = QStringLiteral("stage_1_action_1");
    action1.commandId = QStringLiteral("WritePurgeFlow");
    action1.params = parseJsonObject(QStringLiteral("{\"flow_l_min\":10}"));
    stage1.actions.append(action1);

    PurgeStageDefinition stage2;
    stage2.name = QStringLiteral("Stage 2");
    stage2.durationSeconds = 570;
    PurgeActionDefinition action2;
    action2.actionId = QStringLiteral("stage_2_action_1");
    action2.commandId = QStringLiteral("WritePurgeFlow");
    action2.params = parseJsonObject(QStringLiteral("{\"flow_l_min\":30}"));
    stage2.actions.append(action2);

    definition.stages.append(stage1);
    definition.stages.append(stage2);
    return definition;
}
