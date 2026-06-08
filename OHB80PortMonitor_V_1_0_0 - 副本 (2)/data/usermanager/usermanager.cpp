#include "usermanager.h"
#include "appconfig.h"

#include <QSettings>
#include <QCoreApplication>
#include <QFile>
#include <QDir>
#include <QFileInfo>
#include <QDebug>

UserManager* UserManager::m_instance = nullptr;

UserManager* UserManager::instance()
{
    if (!m_instance) {
        m_instance = new UserManager();
    }
    return m_instance;
}

UserManager::UserManager(QObject* parent)
    : QObject(parent)
    , m_currentPermission(UserPermission::Guest)
{
    initDefaultConfig();
}

// 若配置文件不存在，则生成默认用户数据
void UserManager::initDefaultConfig()
{
    const QString path = configFilePath();
    if (QFile::exists(path)) return;

    // 确保目录存在
    const QDir dir = QFileInfo(path).absoluteDir();
    if (!dir.exists()) {
        dir.mkpath(".");
    }

    QSettings s(path, QSettings::IniFormat);
    s.setIniCodec("UTF-8");

    s.beginGroup("cytc");
    s.setValue("password",   "cytc");
    s.setValue("permission", static_cast<int>(UserPermission::Engineer));
    s.endGroup();

    s.beginGroup("debug");
    s.setValue("password",   "debug");
    s.setValue("permission", static_cast<int>(UserPermission::Debug));
    s.endGroup();

    s.beginGroup("user1");
    s.setValue("password",   "user123");
    s.setValue("permission", static_cast<int>(UserPermission::Normal));
    s.endGroup();

    // 显式同步确保数据写入磁盘
    s.sync();
}

void UserManager::registerWidget(QWidget* widget, UserPermission minPermission)
{
    if (!widget) return;
    m_widgetPermMap[minPermission].append(widget);
    widget->setVisible(m_currentPermission >= minPermission);
}

bool UserManager::login(const QString& username, const QString& password)
{
    // 内置账号验证
    if (username == "root") {
        if (password == "cytc666666") {
            m_currentUser       = username;
            m_currentPermission = UserPermission::Root;
            updateWidgetVisibility();
            emit loginSuccess(username, m_currentPermission);
            emit permissionChanged(m_currentPermission);
            return true;
        } else {
            emit loginFailed(tr("密码错误"));
            return false;
        }
    }

    // 从配置文件读取其他账号
    QSettings s(configFilePath(), QSettings::IniFormat);
    s.setIniCodec("UTF-8");

    s.beginGroup(username);
    const QString storedPwd = s.value("password").toString();
    const int     permValue = s.value("permission", 0).toInt();
    s.endGroup();

    if (storedPwd.isEmpty()) {
        emit loginFailed(tr("用户不存在"));
        return false;
    }
    if (storedPwd != password) {
        emit loginFailed(tr("密码错误"));
        return false;
    }

    m_currentUser       = username;
    m_currentPermission = static_cast<UserPermission>(permValue);

    updateWidgetVisibility();
    emit loginSuccess(username, m_currentPermission);
    emit permissionChanged(m_currentPermission);
    return true;
}

void UserManager::logout()
{
    m_currentUser.clear();
    m_currentPermission = UserPermission::Guest;
    updateWidgetVisibility();
    emit logoutSuccess();
    emit permissionChanged(m_currentPermission);
}

bool UserManager::addUser(const QString& username, const QString& password, UserPermission permission)
{
    if (!hasPermission(UserPermission::Debug)) {
        qWarning() << "[UserManager] addUser: 权限不足";
        return false;
    }

    QSettings s(configFilePath(), QSettings::IniFormat);
    s.setIniCodec("UTF-8");

    s.beginGroup(username);
    const bool exists = s.contains("password");
    s.endGroup();

    if (exists) {
        qWarning() << "[UserManager] addUser: 用户已存在 -" << username;
        return false;
    }

    s.beginGroup(username);
    s.setValue("password",   password);
    s.setValue("permission", static_cast<int>(permission));
    s.endGroup();
    return true;
}

bool UserManager::removeUser(const QString& username)
{
    if (!hasPermission(UserPermission::Debug)) {
        qWarning() << "[UserManager] removeUser: 权限不足";
        return false;
    }

    QSettings s(configFilePath(), QSettings::IniFormat);
    s.setIniCodec("UTF-8");

    s.beginGroup(username);
    const bool exists = s.contains("password");
    s.endGroup();

    if (!exists) {
        qWarning() << "[UserManager] removeUser: 用户不存在 -" << username;
        return false;
    }

    s.remove(username);
    return true;
}

bool UserManager::modifyUser(const QString& username, const QString& newPassword)
{
    if (!hasPermission(UserPermission::Debug)) {
        qWarning() << "[UserManager] modifyUser: 权限不足";
        return false;
    }

    QSettings s(configFilePath(), QSettings::IniFormat);
    s.setIniCodec("UTF-8");

    s.beginGroup(username);
    const bool exists = s.contains("password");
    s.endGroup();

    if (!exists) {
        qWarning() << "[UserManager] modifyUser: 用户不存在 -" << username;
        return false;
    }

    s.beginGroup(username);
    s.setValue("password", newPassword);
    s.endGroup();
    return true;
}

bool UserManager::setUserPermission(const QString& username, UserPermission newPermission)
{
    if (!hasPermission(UserPermission::Debug)) {
        qWarning() << "[UserManager] setUserPermission: 权限不足";
        return false;
    }

    QSettings s(configFilePath(), QSettings::IniFormat);
    s.setIniCodec("UTF-8");

    s.beginGroup(username);
    const bool exists = s.contains("password");
    s.endGroup();

    if (!exists) {
        qWarning() << "[UserManager] setUserPermission: 用户不存在 -" << username;
        return false;
    }

    s.beginGroup(username);
    s.setValue("permission", static_cast<int>(newPermission));
    s.endGroup();
    return true;
}

QString UserManager::currentUser() const
{
    return m_currentUser;
}

UserPermission UserManager::currentPermission() const
{
    return m_currentPermission;
}

QStringList UserManager::allUsers() const
{
    QSettings s(configFilePath(), QSettings::IniFormat);
    s.setIniCodec("UTF-8");
    return s.childGroups();
}

UserPermission UserManager::userPermission(const QString& username) const
{
    QSettings s(configFilePath(), QSettings::IniFormat);
    s.setIniCodec("UTF-8");

    s.beginGroup(username);
    const int val = s.value("permission", 0).toInt();
    s.endGroup();
    return static_cast<UserPermission>(val);
}

void UserManager::updateWidgetVisibility()
{
    // QMap 按 key 升序排列，利用 upperBound 将 map 分为两段：
    //   [begin,  cutoff) → key <= currentPermission → 显示
    //   [cutoff, end)    → key >  currentPermission → 隐藏
    const auto cutoff = m_widgetPermMap.upperBound(m_currentPermission);

    for (auto it = m_widgetPermMap.constBegin(); it != cutoff; ++it) {
        for (QWidget* w : it.value()) {
            if (w) w->show();
        }
    }
    for (auto it = cutoff; it != m_widgetPermMap.constEnd(); ++it) {
        for (QWidget* w : it.value()) {
            if (w) w->hide();
        }
    }
}

bool UserManager::hasPermission(UserPermission required) const
{
    return m_currentPermission >= required;
}

QString UserManager::configFilePath() const
{
    return AppConfig::getInstance().getUserInfoConfigPath();
}

QString UserManager::permissionToString(UserPermission perm)
{
    switch (perm) {
        case UserPermission::Guest:    return QStringLiteral("Guest");
        case UserPermission::Normal:   return QStringLiteral("Normal");
        case UserPermission::Debug:    return QStringLiteral("Debug");
        case UserPermission::Engineer: return QStringLiteral("Engineer");
        case UserPermission::Root:     return QStringLiteral("Root");
        default:                       return QStringLiteral("Unknown");
    }
}
