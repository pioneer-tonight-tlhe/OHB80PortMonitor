#ifndef USERINFOCONFIG_H
#define USERINFOCONFIG_H

#include <QString>
#include <QVector>

struct UserInfo
{
    QString username;
    QString password;
    int permission;

    UserInfo() : permission(0) {}
    UserInfo(const QString& user, const QString& pwd, int perm)
        : username(user), password(pwd), permission(perm) {}
};

class UserInfoConfig
{
public:
    static UserInfoConfig& getInstance();

    QVector<UserInfo> readUserInfo() const;
    bool writeUserInfo(const QVector<UserInfo>& users);

    QString getUserInfoConfigPath() const;

private:
    UserInfoConfig();
    ~UserInfoConfig() = default;
    UserInfoConfig(const UserInfoConfig&) = delete;
    UserInfoConfig& operator=(const UserInfoConfig&) = delete;

    QString m_configFilePath;
};

#endif // USERINFOCONFIG_H
