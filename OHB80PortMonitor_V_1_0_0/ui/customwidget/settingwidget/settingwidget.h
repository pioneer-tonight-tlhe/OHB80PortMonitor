/*******************************************************************************************
 * @file settingwidget.h
 * @author OpenAI Codex <工号：AI> 2026-06-18
 *
 * @class SettingWidget
 * @brief 设置分组控件类，负责承载标题栏、设置项列表和自定义控件区域。
 *
 * 设计目标：
 *      1. 为多条设置项提供统一的分组容器和布局入口。
 *      2. 统一管理标题栏的展开收起交互，降低页面侧重复实现成本。
 *      3. 同时兼容标准 SettingItemWidget 和自定义 QWidget 两种接入方式。
 *******************************************************************************************/
#ifndef SETTINGWIDGET_H
#define SETTINGWIDGET_H

#include <QList>
#include <QMap>
#include <QString>
#include <QWidget>

class QEvent;
class QLabel;
class QVBoxLayout;
class SettingItemWidget;

class SettingWidget : public QWidget
{
    Q_OBJECT

public:
    // ============================ 公共数据类型 ============================
    enum CustomWidgetLayout {
        VerticalLayout,
        HorizontalLayout
    };

    // ============================ 构造函数 ============================
    explicit SettingWidget(QWidget *parent = nullptr);
    ~SettingWidget() override;

    // ============================ 标题管理 ============================
    void setTitle(const QString &title);
    QLabel *titleLabel() const;

    // ============================ 标准设置项管理 ============================
    void addItem(SettingItemWidget *item);
    void removeItem(SettingItemWidget *item);
    void removeItemAt(int index);
    void hideItem(int index);
    void showItem(int index);
    void setItem(int index, SettingItemWidget *item);
    SettingItemWidget *itemAt(int index) const;
    int itemCount() const;
    void clearItems();

    // ============================ 自定义控件管理 ============================
    void addCustomWidget(QWidget *customWidget);
    void addCustomWidget(QWidget *customWidget, CustomWidgetLayout layoutType);
    void removeCustomWidget(QWidget *customWidget);
    void clearCustomWidgets();
    void setCustomWidgetLayout(QWidget *customWidget, CustomWidgetLayout layoutType);
    CustomWidgetLayout getCustomWidgetLayout(QWidget *customWidget) const;

protected:
    // ============================ 基类相关接口 ============================
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    // ============================ 界面构建 ============================
    void initUI();
    void updateTitleLabel();

    // ============================ 交互控制 ============================
    void toggleItemsVisibility();

private:
    // ---- 状态成员 ----
    QString m_title;                                  // 当前分组标题文本。
    QLabel *m_titleLabel;                             // 分组标题栏对应的标签控件。
    bool m_itemsVisible;                              // 当前分组内容区是否处于可见状态。

    // ---- 功能模块成员 ----
    QVBoxLayout *m_mainLayout;                        // 整个分组控件的主垂直布局。
    QVBoxLayout *m_itemsLayout;                       // 标准设置项区域使用的垂直布局。
    QVBoxLayout *m_customWidgetsLayout;               // 自定义控件区域使用的垂直布局。
    QList<SettingItemWidget *> m_items;               // 当前分组中管理的标准设置项列表。
    QList<QWidget *> m_customWidgets;                 // 当前分组中管理的自定义控件列表。
    QMap<QWidget *, CustomWidgetLayout> m_customWidgetLayouts; // 自定义控件到布局模式的映射表。
};

#endif // SETTINGWIDGET_H
