#ifndef CHANGEPASSWORDDIALOG_H
#define CHANGEPASSWORDDIALOG_H

#include <QDialog>
#include <QLineEdit>
#include <QPushButton>
#include <QLabel>

class ChangePasswordDialog : public QDialog
{
    Q_OBJECT

public:
    explicit ChangePasswordDialog(QWidget *parent = nullptr);

private slots:
    void onChangePasswordClicked();
    void onCancelClicked();

private:
    void setupUI();
    void connectSignals();

    QLineEdit *m_usernameEdit;
    QLineEdit *m_oldPasswordEdit;
    QLineEdit *m_newPasswordEdit;
    QLineEdit *m_confirmPasswordEdit;
    QPushButton *m_changeBtn;
    QPushButton *m_cancelBtn;
    QLabel *m_errorLabel;
};

#endif // CHANGEPASSWORDDIALOG_H
