#ifndef WAITDIALOG_H
#define WAITDIALOG_H

#include <QDialog>

class QLabel;
class QPushButton;

// 通用等待对话框：
//   - Waiting：显示提示文本 + "取消"按钮（点击发出 cancelRequested 信号）
//   - Success：显示提示文本 + "确认"按钮（点击关闭对话框）
//   - Failure：显示提示文本 + "确认"按钮（点击关闭对话框）
//
// 设计为可复用模块，任意 widget/操作均可在执行耗时任务时使用。
class WaitDialog : public QDialog
{
    Q_OBJECT

public:
    explicit WaitDialog(QWidget *parent = nullptr);

    void setWaiting(const QString& message);
    void setSuccess(const QString& message);
    void setFailure(const QString& message);

signals:
    // 用户在 Waiting 状态下点击了取消按钮
    void cancelRequested();

private slots:
    void onButtonClicked();

private:
    enum class Mode { Waiting, Success, Failure };

    Mode          m_mode;
    QLabel*       m_label;
    QPushButton*  m_button;
};

#endif // WAITDIALOG_H
