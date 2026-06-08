#ifndef SETTINGWIDGET_H
#define SETTINGWIDGET_H

#include <QHBoxLayout>
#include <QWidget>
#include <QMouseEvent>

class QLabel;
class SettingItemWidget;
class QHBoxLayout;
class QVBoxLayout;

class SettingWidget : public QWidget
{
    Q_OBJECT

public:
    // 自定义控件布局类型
    enum CustomWidgetLayout {
        VerticalLayout,   // 垂直布局（默认）
        HorizontalLayout  // 水平布局
    };
    
    explicit SettingWidget(QWidget *parent = nullptr);
    ~SettingWidget();

    // 设置标题
    void setTitle(const QString &title);
    
    // 获取标题控件
    QLabel *titleLabel() const;
    
    // 添加设置项
    void addItem(SettingItemWidget *item);
    
    // 移除设置项
    void removeItem(SettingItemWidget *item);
    
    // 移除指定索引的设置项
    void removeItemAt(int index);
    
    // 隐藏指定索引的设置项
    void hideItem(int index);
    
    // 显示指定索引的设置项
    void showItem(int index);
    
    // 设置指定索引的设置项
    void setItem(int index, SettingItemWidget *item);
    
    // 获取指定索引的设置项
    SettingItemWidget *itemAt(int index) const;
    
    // 获取设置项数量
    int itemCount() const;
    
    // 清空所有设置项
    void clearItems();
    
    // 添加自定义控件（使用默认垂直布局）
    void addCustomWidget(QWidget *customWidget);
    
    // 添加自定义控件（指定布局类型）
    void addCustomWidget(QWidget *customWidget, CustomWidgetLayout layoutType);
    
    // 移除自定义控件
    void removeCustomWidget(QWidget *customWidget);
    
    // 清空所有自定义控件
    void clearCustomWidgets();
    
    // 设置自定义控件的布局类型
    void setCustomWidgetLayout(QWidget *customWidget, CustomWidgetLayout layoutType);
    
    // 获取自定义控件的布局类型
    CustomWidgetLayout getCustomWidgetLayout(QWidget *customWidget) const;

protected:
    void mouseDoubleClickEvent(QMouseEvent *event) override;

private:
    void initUI();
    void toggleItemsVisibility();

private:
    QLabel *m_titleLabel;
    QVBoxLayout *m_mainLayout;
    QVBoxLayout *m_itemsLayout;
    QVBoxLayout *m_customWidgetsLayout;  // 自定义控件布局
    QList<SettingItemWidget*> m_items;
    QList<QWidget*> m_customWidgets;     // 自定义控件列表
    QMap<QWidget*, CustomWidgetLayout> m_customWidgetLayouts;  // 自定义控件布局类型映射
    bool m_itemsVisible;  // 子项可见状态
};

#endif // SETTINGWIDGET_H
