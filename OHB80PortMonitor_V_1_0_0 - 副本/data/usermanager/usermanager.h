#ifndef USERMANAGER_H
#define USERMANAGER_H

#include <QObject>
#include <QMap>
#include <QPointer>
#include <QVector>
#include <QWidget>

// 权限级别枚举（数值越大权限越高，可直接比较大小）
enum UserPermission : int {
    Guest    = 0,   // 游客
    Normal   = 1,   // 普通用户
    Debug    = 2,   // 调试用户
    Engineer = 3,   // 工程师
    Root     = 4    // root 用户
};

class UserManager : public QObject
{
    Q_OBJECT

public:
    // 获取单例实例
    static UserManager* instance();

    // 注册控件：该控件可见所需的最低权限
    void registerWidget(QWidget* widget, UserPermission minPermission);

    // 用户登录，成功返回 true
    bool login(const QString& username, const QString& password);

    // 用户登出，权限恢复为游客
    void logout();

    // 添加用户（需要 Debug 及以上权限）
    bool addUser(const QString& username, const QString& password, UserPermission permission);

    // 删除用户（需要 Debug 及以上权限）
    bool removeUser(const QString& username);

    // 修改用户密码（需要 Debug 及以上权限）
    bool modifyUser(const QString& username, const QString& newPassword);

    // 修改用户权限（需要 Debug 及以上权限）
    bool setUserPermission(const QString& username, UserPermission newPermission);

    // 获取当前登录用户名（未登录返回空字符串）
    QString currentUser() const;

    // 获取当前权限级别
    UserPermission currentPermission() const;

    // 返回配置文件中所有用户名
    QStringList allUsers() const;

    // 返回指定用户的权限级别（用户不存在返回 Guest）
    UserPermission userPermission(const QString& username) const;

    // 将权限枚举转换为字符串
    static QString permissionToString(UserPermission perm);

signals:
    void permissionChanged(UserPermission permission);
    void loginSuccess(const QString& username, UserPermission permission);
    void loginFailed(const QString& reason);
    void logoutSuccess();

private:
    explicit UserManager(QObject* parent = nullptr);
    ~UserManager() = default;
    Q_DISABLE_COPY(UserManager)

    void   initDefaultConfig();
    void   updateWidgetVisibility();
    bool   hasPermission(UserPermission required) const;
    QString configFilePath() const;

    static UserManager* m_instance;

    UserPermission m_currentPermission;
    QString        m_currentUser;

    // key: 可见所需的最低权限，value: 该权限级别下注册的控件列表
    // 使用 QPointer 确保控件销毁后指针自动置 null，防止野指针崩溃
    QMap<UserPermission, QVector<QPointer<QWidget>>> m_widgetPermMap;
};

#endif // USERMANAGER_H
