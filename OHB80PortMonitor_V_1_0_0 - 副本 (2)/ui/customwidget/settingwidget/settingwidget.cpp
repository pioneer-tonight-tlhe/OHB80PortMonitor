#include "settingwidget.h"
#include "settingitemwidget.h"

#include <QLabel>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QMap>

SettingWidget::SettingWidget(QWidget *parent)
    : QWidget(parent)
    , m_titleLabel(nullptr)
    , m_mainLayout(nullptr)
    , m_itemsLayout(nullptr)
    , m_customWidgetsLayout(nullptr)
    , m_itemsVisible(true)
{
    initUI();
}

SettingWidget::~SettingWidget()
{
    // Qt 会自动删除子控件
}

void SettingWidget::initUI()
{
    // 创建主垂直布局
    m_mainLayout = new QVBoxLayout(this);
    m_mainLayout->setContentsMargins(0, 0, 0, 0);
    m_mainLayout->setSpacing(0);
    
    // 创建标题控件
    m_titleLabel = new QLabel(this);
    m_titleLabel->setObjectName("titleLabel");
    m_titleLabel->setStyleSheet(
        "QLabel#titleLabel {"
        "  font-size: 16px;"
        "  font-weight: bold;"
        "  color: #ABD6FF;"
        "  padding: 4px 5px;"
        "  background-color: rgba(0, 50, 107, 180);"
        "  border-left: 3px solid #00B7DE;"
        "  border-bottom: 1px solid rgba(171, 214, 255, 30);"
        "}");
    m_titleLabel->setFixedHeight(30);
    
    // 将标题添加到主布局
    m_mainLayout->addWidget(m_titleLabel);
    
    // 直接在主布局中添加设置项布局
    m_itemsLayout = new QVBoxLayout();
    m_itemsLayout->setContentsMargins(0, 0, 0, 0);
    m_itemsLayout->setSpacing(0);
    
    // 将设置项布局添加到主布局
    m_mainLayout->addLayout(m_itemsLayout);
    
    // 创建自定义控件布局
    m_customWidgetsLayout = new QVBoxLayout();
    m_customWidgetsLayout->setContentsMargins(0, 0, 0, 0);
    m_customWidgetsLayout->setSpacing(0);
    m_mainLayout->addLayout(m_customWidgetsLayout);
    
    // 设置默认标题
    setTitle("Settings");
    
    // 设置标题栏可点击
    m_titleLabel->setCursor(Qt::PointingHandCursor);
}

void SettingWidget::setTitle(const QString &title)
{
    if (m_titleLabel) {
        m_titleLabel->setText(title);
    }
}

QLabel *SettingWidget::titleLabel() const
{
    return m_titleLabel;
}

void SettingWidget::addItem(SettingItemWidget *item)
{
    if (!item) {
        return;
    }
    
    // 避免重复添加
    if (m_items.contains(item)) {
        return;
    }
    
    // 添加到布局和列表
    m_itemsLayout->addWidget(item);
    m_items.append(item);
}

void SettingWidget::removeItem(SettingItemWidget *item)
{
    if (!item) {
        return;
    }
    
    // 从布局中移除
    m_itemsLayout->removeWidget(item);
    
    // 从列表中移除
    m_items.removeOne(item);
    
    // 删除控件（如果需要）
    item->setParent(nullptr);
    delete item;
}

void SettingWidget::removeItemAt(int index)
{
    if (index < 0 || index >= m_items.count()) {
        return;
    }
    
    SettingItemWidget *item = m_items.at(index);
    removeItem(item);
}

void SettingWidget::hideItem(int index)
{
    if (index < 0 || index >= m_items.count()) {
        return;
    }
    
    SettingItemWidget *item = m_items.at(index);
    if (item) {
        item->hide();
    }
}

void SettingWidget::showItem(int index)
{
    if (index < 0 || index >= m_items.count()) {
        return;
    }
    
    SettingItemWidget *item = m_items.at(index);
    if (item) {
        item->show();
    }
}

void SettingWidget::setItem(int index, SettingItemWidget *item)
{
    if (!item || index < 0) {
        return;
    }
    
    // 如果索引超出范围，添加到末尾
    if (index >= m_items.count()) {
        addItem(item);
        return;
    }
    
    // 移除原有项目
    SettingItemWidget *oldItem = m_items.at(index);
    if (oldItem) {
        m_itemsLayout->removeWidget(oldItem);
        oldItem->setParent(nullptr);
        delete oldItem;
    }
    
    // 插入新项目
    m_itemsLayout->insertWidget(index, item);
    m_items.replace(index, item);
}

SettingItemWidget *SettingWidget::itemAt(int index) const
{
    if (index < 0 || index >= m_items.count()) {
        return nullptr;
    }
    
    return m_items.at(index);
}

int SettingWidget::itemCount() const
{
    return m_items.count();
}

void SettingWidget::clearItems()
{
    // 移除所有设置项
    while (!m_items.isEmpty()) {
        removeItem(m_items.first());
    }
}

void SettingWidget::mouseDoubleClickEvent(QMouseEvent *event)
{
    // 检查双击是否在标题栏区域
    if (m_titleLabel && m_titleLabel->geometry().contains(event->pos())) {
        toggleItemsVisibility();
    }
    
    QWidget::mouseDoubleClickEvent(event);
}

void SettingWidget::toggleItemsVisibility()
{
    m_itemsVisible = !m_itemsVisible;
    
    // 切换所有设置项的可见性
    for (SettingItemWidget *item : m_items) {
        if (item) {
            item->setVisible(m_itemsVisible);
        }
    }
    
    // 切换所有自定义控件的可见性
    for (QWidget *widget : m_customWidgets) {
        if (widget) {
            widget->setVisible(m_itemsVisible);
        }
    }
    
    // 调整控件大小
    adjustSize();
}

void SettingWidget::addCustomWidget(QWidget *customWidget)
{
    addCustomWidget(customWidget, VerticalLayout);  // 默认使用垂直布局
}

void SettingWidget::addCustomWidget(QWidget *customWidget, CustomWidgetLayout layoutType)
{
    if (!customWidget) {
        return;
    }
    
    // 避免重复添加
    if (m_customWidgets.contains(customWidget)) {
        return;
    }
    
    // 根据布局类型创建容器
    QWidget *container = nullptr;
    if (layoutType == HorizontalLayout) {
        QHBoxLayout *hLayout = new QHBoxLayout();
        hLayout->setContentsMargins(0, 0, 0, 0);
        hLayout->setSpacing(0);
        hLayout->addWidget(customWidget);
        
        container = new QWidget();
        container->setLayout(hLayout);
    } else {
        // VerticalLayout - 直接添加到主布局
        m_customWidgetsLayout->addWidget(customWidget);
        container = customWidget;
    }
    
    // 添加到列表和映射
    m_customWidgets.append(customWidget);
    m_customWidgetLayouts.insert(customWidget, layoutType);
    
    // 根据当前状态设置可见性
    if (container) {
        container->setVisible(m_itemsVisible);
    }
}

void SettingWidget::removeCustomWidget(QWidget *customWidget)
{
    if (!customWidget) {
        return;
    }
    
    // 获取布局类型
    CustomWidgetLayout layoutType = m_customWidgetLayouts.value(customWidget, VerticalLayout);
    
    // 根据布局类型进行移除
    if (layoutType == HorizontalLayout) {
        // 水平布局需要移除容器
        QWidget *container = customWidget->parentWidget();
        if (container) {
            m_customWidgetsLayout->removeWidget(container);
            container->setParent(nullptr);
            delete container;  // 删除容器，但不删除自定义控件
        }
    } else {
        // 垂直布局直接移除
        m_customWidgetsLayout->removeWidget(customWidget);
    }
    
    // 从列表和映射中移除
    m_customWidgets.removeOne(customWidget);
    m_customWidgetLayouts.remove(customWidget);
    
    // 设置父控件为空，但不删除（让调用者管理生命周期）
    customWidget->setParent(nullptr);
}

void SettingWidget::clearCustomWidgets()
{
    // 移除所有自定义控件
    while (!m_customWidgets.isEmpty()) {
        QWidget *widget = m_customWidgets.first();
        removeCustomWidget(widget);
    }
}

void SettingWidget::setCustomWidgetLayout(QWidget *customWidget, CustomWidgetLayout layoutType)
{
    if (!customWidget || !m_customWidgets.contains(customWidget)) {
        return;
    }
    
    // 如果布局类型相同，无需更改
    CustomWidgetLayout currentLayout = m_customWidgetLayouts.value(customWidget, VerticalLayout);
    if (currentLayout == layoutType) {
        return;
    }
    
    // 先移除，再重新添加
    bool visible = customWidget->isVisible();
    removeCustomWidget(customWidget);
    addCustomWidget(customWidget, layoutType);
    
    // 恢复可见性
    customWidget->setVisible(visible);
}

SettingWidget::CustomWidgetLayout SettingWidget::getCustomWidgetLayout(QWidget *customWidget) const
{
    if (!customWidget || !m_customWidgets.contains(customWidget)) {
        return VerticalLayout;  // 默认返回垂直布局
    }
    
    return m_customWidgetLayouts.value(customWidget, VerticalLayout);
}
