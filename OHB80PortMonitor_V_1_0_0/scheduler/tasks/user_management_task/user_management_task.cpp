#include "user_management_task.h"
#include "app/applogger.h"
#include "loggermanager.h"
#include "defer/defer.h"

#include <QDebug>

UserManagementTask::UserManagementTask(QObject *parent)
    : SchedulerTask(parent)
{
}

// ============================================================
// 配置接口
// ============================================================

void UserManagementTask::setLogin(const QString& username, const QString& password)
{
    m_operation = Operation::Login;
    m_username  = username;
    m_password  = password;
}

void UserManagementTask::setLogout()
{
    m_operation = Operation::Logout;
}

void UserManagementTask::setChangePassword(const QString& username, const QString& newPassword)
{
    m_operation   = Operation::ChangePassword;
    m_username    = username;
    m_newPassword = newPassword;
}

void UserManagementTask::setAddUser(const QString& username, const QString& password, UserPermission permission)
{
    m_operation  = Operation::AddUser;
    m_username   = username;
    m_password   = password;
    m_permission = permission;
}

void UserManagementTask::setRemoveUser(const QString& username)
{
    m_operation = Operation::RemoveUser;
    m_username  = username;
}

// ============================================================
// SchedulerTask 接口
// ============================================================

void UserManagementTask::start()
{
    Tool::Defer defer([this]() { LoggerManager::getInstance()->flush(m_taskLogPath); });
    
    if (m_stopped) return;
    setState(Running);
    
    QString opStr;
    switch (m_operation) {
    case Operation::Login:          opStr = "Login"; break;
    case Operation::Logout:         opStr = "Logout"; break;
    case Operation::ChangePassword: opStr = "ChangePassword"; break;
    case Operation::AddUser:        opStr = "AddUser"; break;
    case Operation::RemoveUser:     opStr = "RemoveUser"; break;
    }
    LoggerManager::getInstance()->log(m_taskLogPath, Level::INFO,
        QString("[UserManagementTask][start] 任务开始，操作=%1").arg(opStr).toStdString());

    switch (m_operation) {
    case Operation::Login:          executeLogin();          break;
    case Operation::Logout:         executeLogout();         break;
    case Operation::ChangePassword: executeChangePassword(); break;
    case Operation::AddUser:        executeAddUser();        break;
    case Operation::RemoveUser:     executeRemoveUser();     break;
    }
}

void UserManagementTask::stop()
{
    Tool::Defer defer([this]() { LoggerManager::getInstance()->flush(m_taskLogPath); });
    
    m_stopped = true;
    setState(Cancelled);
    emit finished(false, QStringLiteral("UserManagementTask: 任务被取消"));
    LoggerManager::getInstance()->log(m_taskLogPath, Level::INFO,
        "[UserManagementTask][stop] 任务被取消");
}

// ============================================================
// 执行实现
// ============================================================

void UserManagementTask::executeLogin()
{
    UserManager* mgr = UserManager::instance();

    // 先声明连接对象
    QMetaObject::Connection connOk;
    QMetaObject::Connection connFail;

    // 临时连接登录信号，捕获结果后断开
    connOk = connect(mgr, &UserManager::loginSuccess,
        this, [this, &connOk, &connFail](const QString& username, UserPermission perm) {
            QObject::disconnect(connOk);
            QObject::disconnect(connFail);
            qDebug() << "[UserManagementTask] 登录成功 user=" << username
                     << "perm=" << UserManager::permissionToString(perm);
            LoggerManager::getInstance()->log(m_taskLogPath, Level::INFO,
                QString("[UserManagementTask][executeLogin] 登录成功 user=%1 perm=%2")
                    .arg(username, UserManager::permissionToString(perm)).toStdString());
            setState(Finished);
            emit loginSucceeded(username, perm);
            emit operationResult(true, QString("登录成功: %1").arg(username));
            emit finished(true, QString("登录成功: %1").arg(username));
        });

    connFail = connect(mgr, &UserManager::loginFailed,
        this, [this, &connOk, &connFail](const QString& reason) {
            QObject::disconnect(connOk);
            QObject::disconnect(connFail);
            qWarning() << "[UserManagementTask] 登录失败:" << reason;
            LoggerManager::getInstance()->log(m_taskLogPath, Level::WARN,
                QString("[UserManagementTask] 登录失败: %1").arg(reason).toStdString());
            setState(Failed);
            emit loginFailed(reason);
            emit operationResult(false, QString("登录失败: %1").arg(reason));
            emit finished(false, QString("登录失败: %1").arg(reason));
        });

    mgr->login(m_username, m_password);
}

void UserManagementTask::executeLogout()
{
    UserManager* mgr = UserManager::instance();

    QMetaObject::Connection conn;
    conn = connect(mgr, &UserManager::logoutSuccess,
        this, [this, &conn]() {
            QObject::disconnect(conn);
            qDebug() << "[UserManagementTask] 登出成功";
            LoggerManager::getInstance()->log(m_taskLogPath, Level::INFO,
                "[UserManagementTask] 登出成功");
            setState(Finished);
            emit logoutSucceeded();
            emit operationResult(true, QStringLiteral("登出成功"));
            emit finished(true, QStringLiteral("登出成功"));
        });

    mgr->logout();
}

void UserManagementTask::executeChangePassword()
{
    const bool ok = UserManager::instance()->modifyUser(m_username, m_newPassword);
    if (ok) {
        qDebug() << "[UserManagementTask] 修改密码成功 user=" << m_username;
        LoggerManager::getInstance()->log(m_taskLogPath, Level::INFO,
            QString("[UserManagementTask] 修改密码成功 user=%1").arg(m_username).toStdString());
        setState(Finished);
        emit operationResult(true, QString("修改密码成功: %1").arg(m_username));
        emit finished(true, QString("修改密码成功: %1").arg(m_username));
    } else {
        const QString msg = QString("修改密码失败: %1（用户不存在或权限不足）").arg(m_username);
        qWarning() << "[UserManagementTask]" << msg;
        LoggerManager::getInstance()->log(m_taskLogPath, Level::WARN,
            QString("[UserManagementTask] %1").arg(msg).toStdString());
        setState(Failed);
        emit operationResult(false, msg);
        emit finished(false, msg);
    }
}

void UserManagementTask::executeAddUser()
{
    const bool ok = UserManager::instance()->addUser(m_username, m_password, m_permission);
    if (ok) {
        qDebug() << "[UserManagementTask] 添加用户成功 user=" << m_username
                 << "perm=" << UserManager::permissionToString(m_permission);
        LoggerManager::getInstance()->log(m_taskLogPath, Level::INFO,
            QString("[UserManagementTask] 添加用户成功 user=%1 perm=%2")
                .arg(m_username, UserManager::permissionToString(m_permission)).toStdString());
        setState(Finished);
        emit operationResult(true, QString("添加用户成功: %1").arg(m_username));
        emit finished(true, QString("添加用户成功: %1").arg(m_username));
    } else {
        const QString msg = QString("添加用户失败: %1（已存在或权限不足）").arg(m_username);
        qWarning() << "[UserManagementTask]" << msg;
        LoggerManager::getInstance()->log(m_taskLogPath, Level::WARN,
            QString("[UserManagementTask] %1").arg(msg).toStdString());
        setState(Failed);
        emit operationResult(false, msg);
        emit finished(false, msg);
    }
}

void UserManagementTask::executeRemoveUser()
{
    const bool ok = UserManager::instance()->removeUser(m_username);
    if (ok) {
        qDebug() << "[UserManagementTask] 删除用户成功 user=" << m_username;
        LoggerManager::getInstance()->log(m_taskLogPath, Level::INFO,
            QString("[UserManagementTask] 删除用户成功 user=%1").arg(m_username).toStdString());
        setState(Finished);
        emit operationResult(true, QString("删除用户成功: %1").arg(m_username));
        emit finished(true, QString("删除用户成功: %1").arg(m_username));
    } else {
        const QString msg = QString("删除用户失败: %1（不存在或权限不足）").arg(m_username);
        qWarning() << "[UserManagementTask]" << msg;
        LoggerManager::getInstance()->log(m_taskLogPath, Level::WARN,
            QString("[UserManagementTask] %1").arg(msg).toStdString());
        setState(Failed);
        emit operationResult(false, msg);
        emit finished(false, msg);
    }
}
