#ifndef SETTINGITEMWIDGET_H
#define SETTINGITEMWIDGET_H

#include <QGridLayout>
#include <QLabel>
#include <QMap>
#include <QVBoxLayout>
#include <QColor>
#include <QPainter>
#include <QStyleOption>
#include <QWidget>
#include <QTimer>

class SettingItemWidget : public QWidget
{
    Q_OBJECT

public:
    explicit SettingItemWidget(QWidget *parent = nullptr);
    ~SettingItemWidget();

    // 设置标题
    void setTitle(const QString &title);
    
    // 设置提示信息
    void setTip(const QString &tip);
    
    // 获取标题控件
    QLabel *getTitleLabel() const;
    
    // 获取提示控件
    QLabel *getTipLabel() const;

    // 添加操作控件
    void addWidget(const QString &key, QWidget *widget);

    // 获取操作控件
    QWidget *getWidget(const QString &key) const;

    // 设置背景颜色
    void setBackgroundColor(const QColor &color);

    // 设置字体大小
    void setFontSize(int titleSize, int tipSize = 12);

    // 设置状态（成功/失败）
    void setStatus(const QString &errorMsg, bool success);

    // 三态独立方法（不触发内部 QMessageBox）
    void setStatusWaiting(const QString &msg = "Waiting...");
    void setStatusOK(const QString &msg = "OK");
    void setStatusFailed(const QString &msg = "Failed");

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    void initUI();

private slots:
    void hideStatusLabel();

private:
    QLabel *m_titleLabel;
    QLabel *m_tipLabel;
    QLabel *m_statusLabel;
    QString m_statusStyle;
    QWidget *m_widgetActions;
    QGridLayout *m_actionsLayout;
    QVBoxLayout *m_mainLayout;

    QMap<QString, QWidget*> m_actions;
    int m_nextColumn;
    QTimer *m_statusTimer;
};

#endif // SETTINGITEMWIDGET_H
