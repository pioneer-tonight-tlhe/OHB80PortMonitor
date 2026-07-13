#ifndef PURGETASKCONFIG_H
#define PURGETASKCONFIG_H

#include "scheduler/tasks/purge_task/purge_task_types.h"

#include <QString>

class PurgeTaskConfig
{
public:
    static PurgeTaskConfig& getInstance();

    QString readDefaultQRCode() const;
    PurgeTaskDefinition readTaskDefinition() const;
    PurgeTaskDefinition readTaskDefinition(const QString &qrCode) const;
    QString getConfigPath() const;

private:
    PurgeTaskConfig();
    ~PurgeTaskConfig() = default;
    PurgeTaskConfig(const PurgeTaskConfig&) = delete;
    PurgeTaskConfig& operator=(const PurgeTaskConfig&) = delete;

    void ensureDefaultConfigFile() const;
    void writeDefaultConfigFile() const;
    PurgeTaskDefinition defaultDefinition(const QString &qrCode = QString()) const;

private:
    QString m_configFilePath;
};

#endif // PURGETASKCONFIG_H
