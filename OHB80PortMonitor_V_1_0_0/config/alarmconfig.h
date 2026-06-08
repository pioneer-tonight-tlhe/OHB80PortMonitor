#ifndef ALARMCONFIG_H
#define ALARMCONFIG_H

#include <QString>
#include <QSet>

class AlarmConfig
{
public:
    static AlarmConfig& getInstance();

    /// 读取被屏蔽的警报类型集合
    QSet<QString> readBlockedAlarms() const;

    /// 设置警报类型是否被屏蔽
    bool setAlarmBlocked(const QString& alarmType, bool blocked);

    /// 检查指定警报类型是否被屏蔽
    bool isAlarmBlocked(const QString& alarmType) const;

    /// 获取配置文件路径
    QString getAlarmConfigPath() const;

private:
    AlarmConfig();
    ~AlarmConfig() = default;
    AlarmConfig(const AlarmConfig&) = delete;
    AlarmConfig& operator=(const AlarmConfig&) = delete;

    QString m_configFilePath;
};

#endif // ALARMCONFIG_H
