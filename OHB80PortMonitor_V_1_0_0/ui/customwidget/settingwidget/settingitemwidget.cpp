#include "settingitemwidget.h"

#include <QLabel>
#include <QGridLayout>
#include <QVBoxLayout>
#include <QWidget>
#include <QPalette>
#include <QSpacerItem>
#include <QLineEdit>
#include <QPushButton>
#include <QMessageBox>
#include <QTimer>

SettingItemWidget::SettingItemWidget(QWidget *parent)
    : QWidget(parent)
    , m_titleLabel(nullptr)
    , m_tipLabel(nullptr)
    , m_statusLabel(nullptr)
    , m_statusStyle()
    , m_widgetActions(nullptr)
    , m_actionsLayout(nullptr)
    , m_mainLayout(nullptr)
    , m_nextColumn(2)
    , m_statusTimer(nullptr)
{
    initUI();
}

SettingItemWidget::~SettingItemWidget()
{
}

void SettingItemWidget::initUI()
{
    // 设置样式：仅有下边框1px
    setStyleSheet("SettingItemWidget { border-bottom: 1px solid rgba(0, 50, 120, 200); border-radius: 6px; }");
//    setBackgroundColor(QColor(1, 86, 155, 30));

    // 创建主垂直布局
    m_mainLayout = new QVBoxLayout(this);
    m_mainLayout->setContentsMargins(10, 8, 10, 8);
    m_mainLayout->setSpacing(5);
    
    // 创建操作区域控件和网格布局
    m_widgetActions = new QWidget(this);
    m_actionsLayout = new QGridLayout(m_widgetActions);
    m_actionsLayout->setContentsMargins(0, 0, 0, 0);
    m_actionsLayout->setSpacing(10);
    
    // 创建标题控件
    m_titleLabel = new QLabel(m_widgetActions);
    m_titleLabel->setObjectName("titleLabel");
//    m_titleLabel->setStyleSheet("QLabel#titleLabel { font-weight: bold; color: #ABD6FF; }");
    
    // 添加标题到网格布局（第0行，第0列）
    m_actionsLayout->addWidget(m_titleLabel, 0, 0);
    
    // 创建状态标签（初始不添加到布局，等控件添加完后再添加）
    m_statusLabel = new QLabel(m_widgetActions);
    m_statusLabel->setObjectName("statusLabel");
    m_statusLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    m_statusLabel->setVisible(false);
    m_statusLabel->setMinimumWidth(200); // 设置最小宽度，允许自适应长文本
    
    // 添加水平弹簧（第0行，第1列）
    m_actionsLayout->addItem(new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum), 0, 1);
    
    // 创建定时器
    m_statusTimer = new QTimer(this);
    m_statusTimer->setSingleShot(true);
    connect(m_statusTimer, &QTimer::timeout, this, &SettingItemWidget::hideStatusLabel);
    
    // 将操作区域添加到主布局
    m_mainLayout->addWidget(m_widgetActions);
    
    // 创建提示控件
    m_tipLabel = new QLabel(this);
    m_tipLabel->setObjectName("tipLabel");
//    m_tipLabel->setStyleSheet("QLabel#tipLabel { color: rgba(171,214,255,150); font-size: 12px; }");
    m_tipLabel->setWordWrap(true);
    m_tipLabel->setMinimumWidth(800);
    
    // 将提示控件添加到主布局
    m_mainLayout->addWidget(m_tipLabel);
    
    // 设置默认文本
    setTitle("Setting Item");
    setTip("Please configure the related parameters");
    setFontSize(14);
}

void SettingItemWidget::setTitle(const QString &title)
{
    if (m_titleLabel) {
        m_titleLabel->setText(title);
    }
}

void SettingItemWidget::setTip(const QString &tip)
{
    if (m_tipLabel) {
        m_tipLabel->setText(tip);
        m_tipLabel->setVisible(!tip.isEmpty());
    }
}

QLabel *SettingItemWidget::getTitleLabel() const
{
    return m_titleLabel;
}

QLabel *SettingItemWidget::getTipLabel() const
{
    return m_tipLabel;
}

void SettingItemWidget::addWidget(const QString &key, QWidget *widget)
{
    if (widget && m_actionsLayout) {
        // 根据控件类型设置最小宽度（允许控件自行扩展）
        if (qobject_cast<QPushButton*>(widget)) {
            widget->setFixedWidth(100);
        } else if (qobject_cast<QLineEdit*>(widget)) {
            widget->setMinimumWidth(200);
        }

        // 先移除状态标签（防止与新控件占据同一列，导致Qt样式缓存损坏）
        if (m_statusLabel && m_actionsLayout->indexOf(m_statusLabel) != -1) {
            m_actionsLayout->removeWidget(m_statusLabel);
        }

        // 添加控件到当前列
        m_actionsLayout->addWidget(widget, 0, m_nextColumn);
        m_nextColumn++;

        // 将状态标签添加到最后一列
        if (m_statusLabel) {
            m_actionsLayout->addWidget(m_statusLabel, 0, m_nextColumn);
        }
    }

    m_actions.insert(key, widget);
}

QWidget *SettingItemWidget::getWidget(const QString &key) const
{
    return m_actions.value(key);
}

void SettingItemWidget::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);
    QStyleOption opt;
    opt.initFrom(this);
    QPainter p(this);
    style()->drawPrimitive(QStyle::PE_Widget, &opt, &p, this);
}

void SettingItemWidget::setFontSize(int titleSize, int tipSize)
{
    if (m_titleLabel) {
        m_titleLabel->setStyleSheet(QString("QLabel#titleLabel { font-weight: bold;font-size: %1px; }").arg(titleSize));
    }
    if (m_tipLabel) {
        m_tipLabel->setStyleSheet(QString("QLabel#tipLabel { font-size: %1px; }").arg(tipSize));
    }
}

void SettingItemWidget::setBackgroundColor(const QColor &color)
{
    setStyleSheet(QString("SettingItemWidget { border-bottom: 1px solid rgba(171,214,255,30); background-color: rgba(%1, %2, %3, %4); border-radius: 5px; }")
                  .arg(color.red()).arg(color.green()).arg(color.blue()).arg(color.alpha()));
}

void SettingItemWidget::setStatus(const QString &errorMsg, bool success)
{
    if (!m_statusLabel || !m_statusTimer) {
        return;
    }
    
    // 停止之前的定时器
    m_statusTimer->stop();
    
    if (success) {
        // 成功：显示绿色对勾，5秒后消失
        const QString okStyle = "QLabel#statusLabel { color: #00FF00; font-weight: bold; font-size: 14px; }";
        if (m_statusStyle != okStyle) {
            m_statusLabel->setStyleSheet(okStyle);
            m_statusStyle = okStyle;
        }
        m_statusLabel->setText("✓ OK");
        m_statusLabel->setVisible(true);
        m_statusTimer->start(5000);
    } else {
        // 失败：显示红色叉，10秒后消失，并弹框显示错误信息
        const QString failedStyle = "QLabel#statusLabel { color: #FF0000; font-weight: bold; font-size: 14px; }";
        if (m_statusStyle != failedStyle) {
            m_statusLabel->setStyleSheet(failedStyle);
            m_statusStyle = failedStyle;
        }
        m_statusLabel->setText("✗ Fail");
        m_statusLabel->setVisible(true);
        m_statusTimer->start(10000);

        // 弹框显示错误信息（非阻塞 + 非模态，避免 QDialog::open() 自动改成 WindowModal 阻塞主窗口）
        auto *mb = new QMessageBox(QMessageBox::Critical, "Error", errorMsg, QMessageBox::Ok, this);
        mb->setAttribute(Qt::WA_DeleteOnClose);
        mb->setModal(false);
        mb->setWindowModality(Qt::NonModal);
        mb->show();
    }
}

void SettingItemWidget::hideStatusLabel()
{
    if (m_statusLabel) {
        m_statusLabel->setVisible(false);
    }
}

void SettingItemWidget::setStatusWaiting(const QString &msg)
{
    if (!m_statusLabel || !m_statusTimer) return;
    m_statusTimer->stop();
    const QString waitingStyle = "QLabel#statusLabel { color: #FFA500; font-weight: bold; font-size: 14px; }";
    if (m_statusStyle != waitingStyle) {
        m_statusLabel->setStyleSheet(waitingStyle);
        m_statusStyle = waitingStyle;
    }
    m_statusLabel->setText("⏳ " + msg);
    m_statusLabel->setVisible(true);
    // Waiting 状态不自动消失，等待后续 setStatusOK / setStatusFailed 覆盖
}

void SettingItemWidget::setStatusOK(const QString &msg)
{
    if (!m_statusLabel || !m_statusTimer) return;
    m_statusTimer->stop();
    const QString okStyle = "QLabel#statusLabel { color: #00FF00; font-weight: bold; font-size: 14px; }";
    if (m_statusStyle != okStyle) {
        m_statusLabel->setStyleSheet(okStyle);
        m_statusStyle = okStyle;
    }
    m_statusLabel->setText("✓ " + msg);
    m_statusLabel->setVisible(true);
    m_statusTimer->start(5000);
}

void SettingItemWidget::setStatusFailed(const QString &msg)
{
    if (!m_statusLabel || !m_statusTimer) return;
    m_statusTimer->stop();
    const QString failedStyle = "QLabel#statusLabel { color: #FF0000; font-weight: bold; font-size: 14px; }";
    if (m_statusStyle != failedStyle) {
        m_statusLabel->setStyleSheet(failedStyle);
        m_statusStyle = failedStyle;
    }
    m_statusLabel->setText("✗ " + msg);
    m_statusLabel->setVisible(true);
    m_statusTimer->start(10000);
}
