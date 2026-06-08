#include "useraccountlabel.h"
#include "logindialog.h"
#include "changepassworddialog.h"
#include "usermanager.h"
#include "scheduler/scheduler.h"
#include "scheduler/tasks/user_management_task.h"

#include <QPixmap>
#include <QMenu>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>

UserAccountLabel::UserAccountLabel(QWidget *parent)
    : QLabel(parent)
    , m_loginState(LoginState::NotLoggedIn)
    , m_contextMenu(nullptr)
    , m_iconSize(30)
{
    setAlignment(Qt::AlignCenter);

    m_originalPixmap.load(":/image/useraccount.png");
    setIconSize(30);

    // 连接 UserManager 信号
    UserManager* mgr = UserManager::instance();
    connect(mgr, &UserManager::loginSuccess, this, [this](const QString& username, UserPermission) {
        setLoginState(LoginState::LoggedIn);
        setCurrentUser(username);
    });
    connect(mgr, &UserManager::logoutSuccess, this, [this]() {
        setLoginState(LoginState::NotLoggedIn);
        setCurrentUser("");
    });
}

void UserAccountLabel::setIconSize(int size)
{
    m_iconSize = size;
    setFixedSize(size, size);
    updateDisplay();
}

void UserAccountLabel::setLoginState(LoginState state)
{
    m_loginState = state;
    updateDisplay();
}

UserAccountLabel::LoginState UserAccountLabel::loginState() const
{
    return m_loginState;
}

void UserAccountLabel::setCurrentUser(const QString& username)
{
    m_currentUser = username;
    updateDisplay();
}

QString UserAccountLabel::currentUser() const
{
    return m_currentUser;
}

void UserAccountLabel::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        showContextMenu();
    }
    QLabel::mousePressEvent(event);
}

void UserAccountLabel::showContextMenu()
{
    if (m_contextMenu) {
        return;  // 菜单已存在，不重复创建
    }

    m_contextMenu = new QMenu(this);
    m_contextMenu->setAttribute(Qt::WA_DeleteOnClose);
    m_contextMenu->setWindowFlag(Qt::FramelessWindowHint);
    m_contextMenu->setAttribute(Qt::WA_TranslucentBackground);

    // 与 QUIWidget LightBlue 主题协调：浅蓝背景、深蓝文字、青色 hover
    static const QString kMenuStyle = QStringLiteral(R"(
        QMenu {
            background-color: #EAF7FF;
            border: 1px solid #C0DCF2;
            border-radius: 8px;
            padding: 6px 0px;
            font-family: "Segoe UI", "Microsoft YaHei", sans-serif;
            font-size: 13px;
            color: #386487;
        }
        QMenu::item {
            padding: 8px 24px;
            margin: 2px 6px;
            border-radius: 6px;
            min-width: 180px;
            min-height: 24px;
            background-color: transparent;
        }
        QMenu::item:selected {
            background-color: #00BB9E;
            color: #FFFFFF;
        }
        QMenu::item:disabled {
            color: #7A9BB8;
            background-color: transparent;
            font-weight: bold;
        }
        QMenu::separator {
            height: 1px;
            background-color: #C0DCF2;
            margin: 6px 12px;
        }
    )");
    m_contextMenu->setStyleSheet(kMenuStyle);

    UserManager* mgr = UserManager::instance();

    if (m_loginState == LoginState::NotLoggedIn) {
        // 未登录状态：显示当前账号级别和登录账号选项
        QAction* permissionAction = m_contextMenu->addAction(
            QStringLiteral("Level:  ") + UserManager::permissionToString(mgr->currentPermission())
        );
        permissionAction->setEnabled(false);  // 禁用点击

        m_contextMenu->addSeparator();

        QAction* loginAction = m_contextMenu->addAction(QStringLiteral("Login"));
        connect(loginAction, &QAction::triggered, this, &UserAccountLabel::onLoginRequested);
    } else {
        // 已登录状态：显示当前用户名、账号级别，以及修改密码、登录新账号、退出登录选项
        QAction* userAction = m_contextMenu->addAction(
            QStringLiteral("Current User:  ") + m_currentUser
        );
        userAction->setEnabled(false);

        QAction* permissionAction = m_contextMenu->addAction(
            QStringLiteral("Level:  ") + UserManager::permissionToString(mgr->currentPermission())
        );
        permissionAction->setEnabled(false);

        m_contextMenu->addSeparator();

        QAction* changePasswordAction = m_contextMenu->addAction(QStringLiteral("Change Password"));
        connect(changePasswordAction, &QAction::triggered, this, &UserAccountLabel::onChangePasswordRequested);

        QAction* loginNewAction = m_contextMenu->addAction(QStringLiteral("Login New Account"));
        connect(loginNewAction, &QAction::triggered, this, &UserAccountLabel::onLoginNewRequested);

        m_contextMenu->addSeparator();

        QAction* logoutAction = m_contextMenu->addAction(QStringLiteral("Logout"));
        connect(logoutAction, &QAction::triggered, this, &UserAccountLabel::onLogoutRequested);
    }

    // 菜单关闭时清空指针
    connect(m_contextMenu, &QMenu::aboutToHide, this, [this]() {
        m_contextMenu = nullptr;
    });

    // 在 label 的正下方弹出菜单，菜单中心与 label 中心对齐
    QPoint pos = mapToGlobal(QPoint(0, height()));
    m_contextMenu->popup(pos);
    // 调整菜单位置，使其中心与 label 中心对齐
    int menuWidth = m_contextMenu->width();
    pos.setX(pos.x() + (width() - menuWidth) / 2);
    m_contextMenu->move(pos);
}

void UserAccountLabel::closeContextMenu()
{
    if (m_contextMenu) {
        m_contextMenu->close();
    }
}

void UserAccountLabel::onLoginRequested()
{
    LoginDialog dlg(this);
    dlg.exec();
}

void UserAccountLabel::onLoginNewRequested()
{
    // 如果已登录，先退出当前账号
    if (m_loginState == LoginState::LoggedIn && !m_currentUser.isEmpty()) {
        auto* task = new UserManagementTask(this);
        task->setLogout();
        connect(task, &UserManagementTask::logoutSucceeded,
                this, [this]() {
                    // 登出成功后显示登录对话框
                    LoginDialog dlg(this);
                    dlg.exec();
                });
        connect(task, &UserManagementTask::finished,
                task, &QObject::deleteLater);

        Scheduler::instance()->submitTask(task);
    } else {
        // 未登录，直接显示登录对话框
        LoginDialog dlg(this);
        dlg.exec();
    }
}

void UserAccountLabel::onLogoutRequested()
{
    auto* task = new UserManagementTask(this);
    task->setLogout();
    connect(task, &UserManagementTask::logoutSucceeded,
            this, [this]() {
            });
    connect(task, &UserManagementTask::finished,
            task, &QObject::deleteLater);

    Scheduler::instance()->submitTask(task);
}

void UserAccountLabel::onChangePasswordRequested()
{
    ChangePasswordDialog dlg(this);
    dlg.exec();
}

void UserAccountLabel::updateDisplay()
{
    if (m_loginState == LoginState::LoggedIn && !m_currentUser.isEmpty()) {
        // 已登录：显示蓝色圆形 + 用户名首字母
        setPixmap(makeAvatarPixmap(m_currentUser, m_iconSize));
    } else {
        // 未登录：显示默认图标
        if (!m_originalPixmap.isNull()) {
            setPixmap(m_originalPixmap.scaled(m_iconSize, m_iconSize,
                                              Qt::KeepAspectRatio,
                                              Qt::SmoothTransformation));
        }
    }
}

QPixmap UserAccountLabel::makeAvatarPixmap(const QString& name, int size) const
{
    // 获取首字母（大写）
    QString firstLetter;
    if (!name.isEmpty()) {
        firstLetter = name.left(1).toUpper();
    }

    // 创建透明背景的 pixmap
    QPixmap pixmap(size, size);
    pixmap.fill(Qt::transparent);

    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setRenderHint(QPainter::TextAntialiasing);

    // 绘制蓝色实心圆
    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(30, 144, 255));  // 道奇蓝
    painter.drawEllipse(0, 0, size, size);

    // 在圆中心绘制首字母
    if (!firstLetter.isEmpty()) {
        QFont font = painter.font();
        font.setPixelSize(size * 0.5);
        font.setBold(true);
        painter.setFont(font);
        painter.setPen(Qt::white);
        painter.drawText(QRect(0, 0, size, size), Qt::AlignCenter, firstLetter);
    }

    return pixmap;
}
