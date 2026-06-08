#ifndef USERACCOUNTLABEL_H
#define USERACCOUNTLABEL_H

#include <QLabel>
#include <QMenu>
#include <QObject>

class UserAccountLabel : public QLabel
{
    Q_OBJECT

public:
    // 登录状态枚举
    enum class LoginState {
        NotLoggedIn,  // 未登录
        LoggedIn      // 已登录
    };

    explicit UserAccountLabel(QWidget *parent = nullptr);

    // 设置图标大小（宽度），高度按比例自动调整
    void setIconSize(int size);

    // 设置登录状态
    void setLoginState(LoginState state);

    // 获取当前登录状态
    LoginState loginState() const;

    // 设置当前登录用户名
    void setCurrentUser(const QString& username);

    // 获取当前登录用户名
    QString currentUser() const;

signals:
    void loginRequested();      // 请求登录
    void loginNewRequested();   // 请求登录新账号
    void logoutRequested();     // 请求登出

protected:
    void mousePressEvent(QMouseEvent *event) override;

private slots:
    void onLoginRequested();
    void onLoginNewRequested();
    void onLogoutRequested();
    void onChangePasswordRequested();

private:
    void showContextMenu();
    void closeContextMenu();
    void updateDisplay();                              // 根据状态更新显示内容
    QPixmap makeAvatarPixmap(const QString& name, int size) const;  // 生成圆形头像 pixmap

    QPixmap m_originalPixmap;   // 原始图片，用于缩放
    LoginState m_loginState;    // 当前登录状态
    QString m_currentUser;      // 当前登录用户名
    QMenu* m_contextMenu;       // 上下文菜单指针
    int m_iconSize;             // 当前图标大小（宽度）
};

#endif // USERACCOUNTLABEL_H
