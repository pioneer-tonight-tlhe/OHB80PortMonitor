#include "settingwidget.h"

#include "settingitemwidget.h"

#include <QEvent>
#include <QHBoxLayout>
#include <QLabel>
#include <QLayout>
#include <QMouseEvent>
#include <QVBoxLayout>

SettingWidget::SettingWidget(QWidget *parent)
    : QWidget(parent)
    , m_title(QStringLiteral("Settings"))
    , m_titleLabel(nullptr)
    , m_itemsVisible(true)
    , m_mainLayout(nullptr)
    , m_itemsLayout(nullptr)
    , m_customWidgetsLayout(nullptr)
{
    initUI();
}

SettingWidget::~SettingWidget() = default;

void SettingWidget::setTitle(const QString &title)
{
    m_title = title;
    updateTitleLabel();
}

QLabel *SettingWidget::titleLabel() const
{
    return m_titleLabel;
}

void SettingWidget::addItem(SettingItemWidget *item)
{
    if (!item || m_items.contains(item)) {
        return;
    }

    m_itemsLayout->addWidget(item);
    m_items.append(item);
    item->setVisible(m_itemsVisible);
}

void SettingWidget::removeItem(SettingItemWidget *item)
{
    if (!item) {
        return;
    }

    m_itemsLayout->removeWidget(item);
    m_items.removeOne(item);
    item->setParent(nullptr);
    delete item;
}

void SettingWidget::removeItemAt(int index)
{
    if (index < 0 || index >= m_items.count()) {
        return;
    }

    removeItem(m_items.at(index));
}

void SettingWidget::hideItem(int index)
{
    if (index < 0 || index >= m_items.count()) {
        return;
    }

    if (SettingItemWidget *item = m_items.at(index)) {
        item->hide();
    }
}

void SettingWidget::showItem(int index)
{
    if (index < 0 || index >= m_items.count()) {
        return;
    }

    if (SettingItemWidget *item = m_items.at(index)) {
        item->show();
    }
}

void SettingWidget::setItem(int index, SettingItemWidget *item)
{
    if (!item || index < 0) {
        return;
    }

    if (index >= m_items.count()) {
        addItem(item);
        return;
    }

    SettingItemWidget *oldItem = m_items.at(index);
    if (oldItem) {
        m_itemsLayout->removeWidget(oldItem);
        oldItem->setParent(nullptr);
        delete oldItem;
    }

    m_itemsLayout->insertWidget(index, item);
    m_items.replace(index, item);
    item->setVisible(m_itemsVisible);
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
    while (!m_items.isEmpty()) {
        removeItem(m_items.first());
    }
}

void SettingWidget::addCustomWidget(QWidget *customWidget)
{
    addCustomWidget(customWidget, VerticalLayout);
}

void SettingWidget::addCustomWidget(QWidget *customWidget, CustomWidgetLayout layoutType)
{
    if (!customWidget || m_customWidgets.contains(customWidget)) {
        return;
    }

    QWidget *container = nullptr;
    if (layoutType == HorizontalLayout) {
        container = new QWidget(this);
        QHBoxLayout *layout = new QHBoxLayout(container);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->setSpacing(0);
        layout->addWidget(customWidget);
        m_customWidgetsLayout->addWidget(container);
    } else {
        container = customWidget;
        m_customWidgetsLayout->addWidget(customWidget);
    }

    m_customWidgets.append(customWidget);
    m_customWidgetLayouts.insert(customWidget, layoutType);

    if (container) {
        container->setVisible(m_itemsVisible);
    }
}

void SettingWidget::removeCustomWidget(QWidget *customWidget)
{
    if (!customWidget) {
        return;
    }

    const CustomWidgetLayout layoutType = m_customWidgetLayouts.value(customWidget, VerticalLayout);
    bool detachedFromContainer = false;

    if (layoutType == HorizontalLayout) {
        QWidget *container = customWidget->parentWidget();
        if (container) {
            if (QLayout *layout = container->layout()) {
                layout->removeWidget(customWidget);
            }

            customWidget->setParent(nullptr);
            detachedFromContainer = true;
            m_customWidgetsLayout->removeWidget(container);
            delete container;
        }
    } else {
        m_customWidgetsLayout->removeWidget(customWidget);
    }

    m_customWidgets.removeOne(customWidget);
    m_customWidgetLayouts.remove(customWidget);

    if (!detachedFromContainer) {
        customWidget->setParent(nullptr);
    }
}

void SettingWidget::clearCustomWidgets()
{
    while (!m_customWidgets.isEmpty()) {
        removeCustomWidget(m_customWidgets.first());
    }
}

void SettingWidget::setCustomWidgetLayout(QWidget *customWidget, CustomWidgetLayout layoutType)
{
    if (!customWidget || !m_customWidgets.contains(customWidget)) {
        return;
    }

    const CustomWidgetLayout currentLayout = m_customWidgetLayouts.value(customWidget, VerticalLayout);
    if (currentLayout == layoutType) {
        return;
    }

    const bool visible = customWidget->isVisible();
    removeCustomWidget(customWidget);
    addCustomWidget(customWidget, layoutType);

    QWidget *targetWidget = customWidget;
    if (layoutType == HorizontalLayout && customWidget->parentWidget()) {
        targetWidget = customWidget->parentWidget();
    }

    targetWidget->setVisible(visible);
}

SettingWidget::CustomWidgetLayout SettingWidget::getCustomWidgetLayout(QWidget *customWidget) const
{
    if (!customWidget || !m_customWidgets.contains(customWidget)) {
        return VerticalLayout;
    }

    return m_customWidgetLayouts.value(customWidget, VerticalLayout);
}

bool SettingWidget::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == m_titleLabel && event->type() == QEvent::MouseButtonPress) {
        QMouseEvent *mouseEvent = static_cast<QMouseEvent *>(event);
        if (mouseEvent->button() == Qt::LeftButton) {
            toggleItemsVisibility();
            return true;
        }
    }

    return QWidget::eventFilter(watched, event);
}

void SettingWidget::initUI()
{
    m_mainLayout = new QVBoxLayout(this);
    m_mainLayout->setContentsMargins(0, 0, 0, 0);
    m_mainLayout->setSpacing(0);

    m_titleLabel = new QLabel(this);
    m_titleLabel->setObjectName(QStringLiteral("titleLabel"));
    m_titleLabel->setFixedHeight(30);
    m_titleLabel->setCursor(Qt::PointingHandCursor);
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
    m_titleLabel->installEventFilter(this);
    m_mainLayout->addWidget(m_titleLabel);

    m_itemsLayout = new QVBoxLayout();
    m_itemsLayout->setContentsMargins(0, 0, 0, 0);
    m_itemsLayout->setSpacing(0);
    m_mainLayout->addLayout(m_itemsLayout);

    m_customWidgetsLayout = new QVBoxLayout();
    m_customWidgetsLayout->setContentsMargins(0, 0, 0, 0);
    m_customWidgetsLayout->setSpacing(0);
    m_mainLayout->addLayout(m_customWidgetsLayout);

    updateTitleLabel();
}

void SettingWidget::updateTitleLabel()
{
    if (!m_titleLabel) {
        return;
    }

    const QString prefix = m_itemsVisible ? QStringLiteral("∨ ") : QStringLiteral("＞ ");
    m_titleLabel->setText(prefix + m_title);
}

void SettingWidget::toggleItemsVisibility()
{
    m_itemsVisible = !m_itemsVisible;

    for (SettingItemWidget *item : m_items) {
        if (item) {
            item->setVisible(m_itemsVisible);
        }
    }

    for (QWidget *widget : m_customWidgets) {
        if (!widget) {
            continue;
        }

        QWidget *targetWidget = widget;
        if (m_customWidgetLayouts.value(widget, VerticalLayout) == HorizontalLayout && widget->parentWidget()) {
            targetWidget = widget->parentWidget();
        }

        targetWidget->setVisible(m_itemsVisible);
    }

    updateTitleLabel();
    adjustSize();
}
