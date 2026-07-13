#include "purge_task_types.h"

bool PurgeTaskDefinition::isValid(QString *errorMessage) const
{
    if (qrCode.trimmed().isEmpty()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Purge task QRCode is empty");
        }
        return false;
    }

    if (stages.isEmpty()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Purge task has no stages");
        }
        return false;
    }

    for (int stageIndex = 0; stageIndex < stages.size(); ++stageIndex) {
        const PurgeStageDefinition &stage = stages.at(stageIndex);
        if (stage.durationSeconds < 0) {
            if (errorMessage) {
                *errorMessage = QStringLiteral("Purge stage %1 duration is invalid")
                                    .arg(stageIndex + 1);
            }
            return false;
        }

        for (int actionIndex = 0; actionIndex < stage.actions.size(); ++actionIndex) {
            const PurgeActionDefinition &action = stage.actions.at(actionIndex);
            if (action.commandId.trimmed().isEmpty()) {
                if (errorMessage) {
                    *errorMessage = QStringLiteral("Purge stage %1 action %2 command id is empty")
                                        .arg(stageIndex + 1)
                                        .arg(actionIndex + 1);
                }
                return false;
            }
        }
    }

    if (errorMessage) {
        errorMessage->clear();
    }
    return true;
}
