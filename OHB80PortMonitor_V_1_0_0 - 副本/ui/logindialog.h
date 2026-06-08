#ifndef LOGINDIALOG_H
#define LOGINDIALOG_H

#include <QDialog>
#include <QLineEdit>
#include <QPushButton>
#include <QLabel>

enum UserPermission : int;

class LoginDialog : public QDialog
{
    Q_OBJECT

public:
    explicit LoginDialog(QWidget *parent = nullptr);

    // 获取输入的用户名
    QString username() const;

    // 获取输入的密码
    QString password() const;

private slots:
    void onLoginClicked();
    void onCancelClicked();
    void onLoginSucceeded(const QString& username, UserPermission permission);
    void onLoginFailed(const QString& reason);

private:
    void setupUI();
    void connectSignals();

    QLineEdit *m_usernameEdit;
    QLineEdit *m_passwordEdit;
    QPushButton *m_loginBtn;
    QPushButton *m_cancelBtn;
    QLabel *m_errorLabel;
};

#endif // LOGINDIALOG_H
