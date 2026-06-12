#ifndef USER_MANAGEMENT_TASK_H
#define USER_MANAGEMENT_TASK_H

#include "../../scheduler_task.h"
#include "usermanager.h"


// ============================================================
// UserManagementTask - 用户管理调度任务（普通单次任务）
//
// 支持操作：Login / Logout / ChangePassword / AddUser / RemoveUser
// 使用前先调用对应的 set 方法配置操作类型及参数，
// 然后由 Scheduler 调用 start() 执行。
// ============================================================
class UserManagementTask : public SchedulerTask
{
    Q_OBJECT

public:
    // 操作类型
    enum class Operation {
        Login,          // 登录
        Logout,         // 登出
        ChangePassword, // 修改密码
        AddUser,        // 添加用户
        RemoveUser      // 删除用户
    };
    Q_ENUM(Operation)

    explicit UserManagementTask(QObject *parent = nullptr);
    ~UserManagementTask() override = default;

    // 配置操作类型及参数（选择其中一个调用）
    void setLogin(const QString& username, const QString& password);
    void setLogout();
    void setChangePassword(const QString& username, const QString& newPassword);
    void setAddUser(const QString& username, const QString& password, UserPermission permission);
    void setRemoveUser(const QString& username);

    // SchedulerTask 接口
    void start() override;
    void stop()  override;
    QString taskType() const override { return QStringLiteral("UserManagementTask"); }

signals:
    // 通用操作结果
    void operationResult(bool success, const QString& message);

    // 登录专用信号
    void loginSucceeded(const QString& username, UserPermission permission);
    void loginFailed(const QString& reason);

    // 登出专用信号
    void logoutSucceeded();

private:
    void executeLogin();
    void executeLogout();
    void executeChangePassword();
    void executeAddUser();
    void executeRemoveUser();

    Operation m_operation = Operation::Logout;

    // 参数
    QString        m_username;
    QString        m_password;
    QString        m_newPassword;
    UserPermission m_permission = UserPermission::Normal;

    bool m_stopped = false;

    const std::string m_taskLogPath = "scheduler/user_management_task";
};

#endif // USER_MANAGEMENT_TASK_H
