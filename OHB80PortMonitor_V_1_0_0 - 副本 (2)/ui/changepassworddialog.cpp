#include "changepassworddialog.h"
#include "usermanager.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QMessageBox>

ChangePasswordDialog::ChangePasswordDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(QStringLiteral("Change Password"));
    setModal(true);
    setMinimumWidth(300);

    setupUI();
    connectSignals();
}

void ChangePasswordDialog::setupUI()
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);

    // 表单布局
    QFormLayout *formLayout = new QFormLayout;

    m_usernameEdit = new QLineEdit;
    m_usernameEdit->setPlaceholderText(QStringLiteral("Enter username"));
    m_usernameEdit->setEnabled(false);  // 用户名只读，自动填充
    formLayout->addRow(QStringLiteral("Username:"), m_usernameEdit);

    m_oldPasswordEdit = new QLineEdit;
    m_oldPasswordEdit->setPlaceholderText(QStringLiteral("Enter old password"));
    m_oldPasswordEdit->setEchoMode(QLineEdit::Password);
    formLayout->addRow(QStringLiteral("Old Password:"), m_oldPasswordEdit);

    m_newPasswordEdit = new QLineEdit;
    m_newPasswordEdit->setPlaceholderText(QStringLiteral("Enter new password"));
    m_newPasswordEdit->setEchoMode(QLineEdit::Password);
    formLayout->addRow(QStringLiteral("New Password:"), m_newPasswordEdit);

    m_confirmPasswordEdit = new QLineEdit;
    m_confirmPasswordEdit->setPlaceholderText(QStringLiteral("Confirm new password"));
    m_confirmPasswordEdit->setEchoMode(QLineEdit::Password);
    formLayout->addRow(QStringLiteral("Confirm Password:"), m_confirmPasswordEdit);

    mainLayout->addLayout(formLayout);

    // 错误提示标签
    m_errorLabel = new QLabel;
    m_errorLabel->setStyleSheet("color: red;");
    m_errorLabel->setVisible(false);
    mainLayout->addWidget(m_errorLabel);

    // 按钮布局
    QHBoxLayout *btnLayout = new QHBoxLayout;
    m_changeBtn = new QPushButton(QStringLiteral("Change"));
    m_cancelBtn = new QPushButton(QStringLiteral("Cancel"));
    btnLayout->addWidget(m_changeBtn);
    btnLayout->addWidget(m_cancelBtn);
    mainLayout->addLayout(btnLayout);

    // 自动填充当前登录用户名
    UserManager* mgr = UserManager::instance();
    m_usernameEdit->setText(mgr->currentUser());
}

void ChangePasswordDialog::connectSignals()
{
    connect(m_changeBtn, &QPushButton::clicked, this, &ChangePasswordDialog::onChangePasswordClicked);
    connect(m_cancelBtn, &QPushButton::clicked, this, &ChangePasswordDialog::onCancelClicked);
}

void ChangePasswordDialog::onChangePasswordClicked()
{
    const QString username = m_usernameEdit->text();
    const QString oldPassword = m_oldPasswordEdit->text();
    const QString newPassword = m_newPasswordEdit->text();
    const QString confirmPassword = m_confirmPasswordEdit->text();

    // 验证输入
    if (username.isEmpty()) {
        m_errorLabel->setText(QStringLiteral("Username cannot be empty"));
        m_errorLabel->setVisible(true);
        return;
    }

    if (oldPassword.isEmpty()) {
        m_errorLabel->setText(QStringLiteral("Old password cannot be empty"));
        m_errorLabel->setVisible(true);
        return;
    }

    if (newPassword.isEmpty()) {
        m_errorLabel->setText(QStringLiteral("New password cannot be empty"));
        m_errorLabel->setVisible(true);
        return;
    }

    if (newPassword != confirmPassword) {
        m_errorLabel->setText(QStringLiteral("Passwords do not match"));
        m_errorLabel->setVisible(true);
        return;
    }

    // 验证旧密码是否正确
    UserManager* mgr = UserManager::instance();
    if (!mgr->login(username, oldPassword)) {
        m_errorLabel->setText(QStringLiteral("Old password is incorrect"));
        m_errorLabel->setVisible(true);
        return;
    }

    // 修改密码
    if (mgr->modifyUser(username, newPassword)) {
        QMessageBox::information(this, QStringLiteral("Success"), QStringLiteral("Password changed successfully"));
        accept();
    } else {
        m_errorLabel->setText(QStringLiteral("Failed to change password"));
        m_errorLabel->setVisible(true);
    }
}

void ChangePasswordDialog::onCancelClicked()
{
    reject();
}
