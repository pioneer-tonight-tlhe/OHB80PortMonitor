/*******************************************************************************************
 * @file settingitemwidget.h
 * @author OpenAI Codex <工号：AI> 2026-06-18
 *
 * @class SettingItemWidget
 * @brief 设置项控件类，负责组织标题、提示信息、操作控件和状态反馈区域。
 *
 * 设计目标：
 *      1. 为单条设置项提供统一的标题、提示和操作区布局结构。
 *      2. 通过 key 映射管理子控件，便于业务层按名称获取操作控件。
 *      3. 统一封装等待、成功和失败三种状态反馈的展示行为。
 *******************************************************************************************/
#ifndef SETTINGITEMWIDGET_H
#define SETTINGITEMWIDGET_H

#include <QMap>
#include <QString>
#include <QWidget>

class QColor;
class QGridLayout;
class QLabel;
class QPaintEvent;
class QTimer;
class QVBoxLayout;

class SettingItemWidget : public QWidget
{
    Q_OBJECT

public:
    // ============================ 构造函数 ============================
    explicit SettingItemWidget(QWidget *parent = nullptr);
    ~SettingItemWidget() override;

    // ============================ 标题与提示 ============================
    void setTitle(const QString &title);
    void setTip(const QString &tip);

    // ============================ 控件访问 ============================
    QLabel *getTitleLabel() const;
    QLabel *getTipLabel() const;

    void addWidget(const QString &key, QWidget *widget);
    QWidget *getWidget(const QString &key) const;

    // ============================ 样式设置 ============================
    void setBackgroundColor(const QColor &color);
    void setFontSize(int titleSize, int tipSize = 12);

    // ============================ 状态反馈 ============================
    void setStatus(const QString &errorMsg, bool success);
    void setStatusWaiting(const QString &msg = "Waiting...");
    void setStatusOK(const QString &msg = "OK");
    void setStatusFailed(const QString &msg = "Failed");

protected:
    // ============================ 基类相关接口 ============================
    void paintEvent(QPaintEvent *event) override;

private:
    // ============================ 界面构建 ============================
    void initUI();

private slots:
    // ---- 状态反馈 ----
    void hideStatusLabel();

private:
    // ---- 状态成员 ----
    QLabel *m_titleLabel;      // 设置项标题标签。
    QLabel *m_tipLabel;        // 设置项提示信息标签。
    QLabel *m_statusLabel;     // 设置项状态反馈标签。
    QString m_statusStyle;     // 当前状态标签使用的样式缓存。
    int m_nextColumn;          // 操作区下一个可插入控件的列索引。

    // ---- 功能模块成员 ----
    QWidget *m_widgetActions;           // 操作区容器控件。
    QGridLayout *m_actionsLayout;       // 操作区网格布局。
    QVBoxLayout *m_mainLayout;          // 设置项主垂直布局。
    QMap<QString, QWidget *> m_actions; // 子控件 key 到控件对象的映射表。
    QTimer *m_statusTimer;              // 状态标签自动隐藏定时器。
};

#endif // SETTINGITEMWIDGET_H
