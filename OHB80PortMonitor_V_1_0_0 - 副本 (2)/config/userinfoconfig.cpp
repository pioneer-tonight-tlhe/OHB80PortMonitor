#include "userinfoconfig.h"
#include "appconfig.h"
#include <QSettings>
#include <QDir>
#include <QDebug>

UserInfoConfig& UserInfoConfig::getInstance()
{
    static UserInfoConfig instance;
    return instance;
}

UserInfoConfig::UserInfoConfig()
{
    QString configDir = AppConfig::getInstance().getConfigDir();
    m_configFilePath = configDir + "/userinfo.ini";
    
    QDir dir;
    if (!dir.exists(configDir)) {
        dir.mkpath(configDir);
    }
}

QString UserInfoConfig::getUserInfoConfigPath() const
{
    return m_configFilePath;
}

QVector<UserInfo> UserInfoConfig::readUserInfo() const
{
    QVector<UserInfo> users;
    
    QSettings settings(m_configFilePath, QSettings::IniFormat);
    
    QStringList groups = settings.childGroups();
    
    for (const QString& username : groups) {
        settings.beginGroup(username);
        QString password = settings.value("password", "").toString();
        int permission = settings.value("permission", 0).toInt();
        settings.endGroup();
        
        if (!password.isEmpty()) {
            users.append(UserInfo(username, password, permission));
        }
    }
    
    qDebug() << "UserInfoConfig: 读取了" << users.size() << "个用户信息";
    return users;
}

bool UserInfoConfig::writeUserInfo(const QVector<UserInfo>& users)
{
    QSettings settings(m_configFilePath, QSettings::IniFormat);
    
    settings.clear();
    
    for (const UserInfo& user : users) {
        settings.beginGroup(user.username);
        settings.setValue("password", user.password);
        settings.setValue("permission", user.permission);
        settings.endGroup();
    }
    
    settings.sync();
    
    qDebug() << "UserInfoConfig: 写入了" << users.size() << "个用户信息到" << m_configFilePath;
    
    return settings.status() == QSettings::NoError;
}
