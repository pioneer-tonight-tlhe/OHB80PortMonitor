#include "logindialog.h"
#include "usermanager.h"
#include "scheduler/scheduler.h"
#include "scheduler/tasks/user_management_task.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QMessageBox>

LoginDialog::LoginDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(QStringLiteral("User Login"));
    setModal(true);
    setMinimumWidth(300);

    setupUI();
    connectSignals();
}

void LoginDialog::setupUI()
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);

    // 表单布局
    QFormLayout *formLayout = new QFormLayout;

    m_usernameEdit = new QLineEdit;
    m_usernameEdit->setPlaceholderText(QStringLiteral("Enter username"));
    formLayout->addRow(QStringLiteral("Username:"), m_usernameEdit);

    m_passwordEdit = new QLineEdit;
    m_passwordEdit->setPlaceholderText(QStringLiteral("Enter password"));
    m_passwordEdit->setEchoMode(QLineEdit::Password);
    formLayout->addRow(QStringLiteral("Password:"), m_passwordEdit);

    mainLayout->addLayout(formLayout);

    // 错误提示标签
    m_errorLabel = new QLabel;
    m_errorLabel->setStyleSheet("color: red;");
    m_errorLabel->setVisible(false);
    mainLayout->addWidget(m_errorLabel);

    // 按钮布局
    QHBoxLayout *btnLayout = new QHBoxLayout;
    m_loginBtn = new QPushButton(QStringLiteral("Login"));
    m_cancelBtn = new QPushButton(QStringLiteral("Cancel"));
    btnLayout->addStretch();
    btnLayout->addWidget(m_loginBtn);
    btnLayout->addWidget(m_cancelBtn);

    mainLayout->addLayout(btnLayout);
    setLayout(mainLayout);
}

void LoginDialog::connectSignals()
{
    connect(m_loginBtn, &QPushButton::clicked, this, &LoginDialog::onLoginClicked);
    connect(m_cancelBtn, &QPushButton::clicked, this, &LoginDialog::onCancelClicked);
}

void LoginDialog::onLoginClicked()
{
    QString user = m_usernameEdit->text().trimmed();
    QString pwd = m_passwordEdit->text();

    if (user.isEmpty()) {
        m_errorLabel->setText(QStringLiteral("Username cannot be empty"));
        m_errorLabel->setVisible(true);
        return;
    }

    if (pwd.isEmpty()) {
        m_errorLabel->setText(QStringLiteral("Password cannot be empty"));
        m_errorLabel->setVisible(true);
        return;
    }

    // 使用 UserManagementTask 进行登录验证
    auto* task = new UserManagementTask(this);
    task->setLogin(user, pwd);

    connect(task, &UserManagementTask::loginSucceeded,
            this, &LoginDialog::onLoginSucceeded);
    connect(task, &UserManagementTask::loginFailed,
            this, &LoginDialog::onLoginFailed);
    connect(task, &UserManagementTask::finished,
            this, [this](bool, const QString&) {
                m_loginBtn->setEnabled(true);
                m_loginBtn->setText(QStringLiteral("Login"));
            });
    connect(task, &UserManagementTask::finished,
            task, &QObject::deleteLater);

    m_loginBtn->setEnabled(false);
    m_loginBtn->setText(QStringLiteral("Logging in..."));

    Scheduler::instance()->submitTask(task);
}

void LoginDialog::onCancelClicked()
{
    reject();
}

void LoginDialog::onLoginSucceeded(const QString& username, UserPermission permission)
{
    Q_UNUSED(permission)
    accept();  // 登录成功，关闭对话框
}

void LoginDialog::onLoginFailed(const QString& reason)
{
    m_errorLabel->setText(reason);
    m_errorLabel->setVisible(true);
    m_passwordEdit->clear();
}

QString LoginDialog::username() const
{
    return m_usernameEdit->text().trimmed();
}

QString LoginDialog::password() const
{
    return m_passwordEdit->text();
}
