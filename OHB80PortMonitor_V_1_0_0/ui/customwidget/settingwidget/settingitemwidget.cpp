#include "settingitemwidget.h"

#include <QColor>
#include <QGridLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPainter>
#include <QPushButton>
#include <QSpacerItem>
#include <QStyle>
#include <QStyleOption>
#include <QTimer>
#include <QVBoxLayout>

SettingItemWidget::SettingItemWidget(QWidget *parent)
    : QWidget(parent)
    , m_titleLabel(nullptr)
    , m_tipLabel(nullptr)
    , m_statusLabel(nullptr)
    , m_statusStyle()
    , m_nextColumn(2)
    , m_widgetActions(nullptr)
    , m_actionsLayout(nullptr)
    , m_mainLayout(nullptr)
    , m_statusTimer(nullptr)
{
    initUI();
}

SettingItemWidget::~SettingItemWidget() = default;

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
    if (!widget || !m_actionsLayout) {
        return;
    }

    if (qobject_cast<QPushButton *>(widget)) {
        widget->setFixedWidth(100);
    } else if (qobject_cast<QLineEdit *>(widget)) {
        widget->setMinimumWidth(200);
    }

    if (m_statusLabel && m_actionsLayout->indexOf(m_statusLabel) != -1) {
        m_actionsLayout->removeWidget(m_statusLabel);
    }

    m_actionsLayout->addWidget(widget, 0, m_nextColumn);
    ++m_nextColumn;

    if (m_statusLabel) {
        m_actionsLayout->addWidget(m_statusLabel, 0, m_nextColumn);
    }

    m_actions.insert(key, widget);
}

QWidget *SettingItemWidget::getWidget(const QString &key) const
{
    return m_actions.value(key);
}

void SettingItemWidget::setBackgroundColor(const QColor &color)
{
    setStyleSheet(QString("SettingItemWidget { border-bottom: 1px solid rgba(171,214,255,30); background-color: rgba(%1, %2, %3, %4); border-radius: 5px; }")
                      .arg(color.red())
                      .arg(color.green())
                      .arg(color.blue())
                      .arg(color.alpha()));
}

void SettingItemWidget::setFontSize(int titleSize, int tipSize)
{
    if (m_titleLabel) {
        m_titleLabel->setStyleSheet(QString("QLabel#titleLabel { font-weight: bold; font-size: %1px; }").arg(titleSize));
    }

    if (m_tipLabel) {
        m_tipLabel->setStyleSheet(QString("QLabel#tipLabel { font-size: %1px; }").arg(tipSize));
    }
}

void SettingItemWidget::setStatus(const QString &errorMsg, bool success)
{
    if (!m_statusLabel || !m_statusTimer) {
        return;
    }

    m_statusTimer->stop();

    if (success) {
        const QString okStyle = QStringLiteral("QLabel#statusLabel { color: #00FF00; font-weight: bold; font-size: 14px; }");
        if (m_statusStyle != okStyle) {
            m_statusLabel->setStyleSheet(okStyle);
            m_statusStyle = okStyle;
        }

        m_statusLabel->setText(QStringLiteral("[OK]"));
        m_statusLabel->setVisible(true);
        m_statusTimer->start(5000);
        return;
    }

    const QString failedStyle = QStringLiteral("QLabel#statusLabel { color: #FF0000; font-weight: bold; font-size: 14px; }");
    if (m_statusStyle != failedStyle) {
        m_statusLabel->setStyleSheet(failedStyle);
        m_statusStyle = failedStyle;
    }

    m_statusLabel->setText(QStringLiteral("[Failed]"));
    m_statusLabel->setVisible(true);
    m_statusTimer->start(10000);

    QMessageBox *messageBox = new QMessageBox(QMessageBox::Critical,
                                              QStringLiteral("Error"),
                                              errorMsg,
                                              QMessageBox::Ok,
                                              this);
    messageBox->setAttribute(Qt::WA_DeleteOnClose);
    messageBox->setModal(false);
    messageBox->setWindowModality(Qt::NonModal);
    messageBox->show();
}

void SettingItemWidget::setStatusWaiting(const QString &msg)
{
    if (!m_statusLabel || !m_statusTimer) {
        return;
    }

    m_statusTimer->stop();

    const QString waitingStyle = QStringLiteral("QLabel#statusLabel { color: #FFA500; font-weight: bold; font-size: 14px; }");
    if (m_statusStyle != waitingStyle) {
        m_statusLabel->setStyleSheet(waitingStyle);
        m_statusStyle = waitingStyle;
    }

    m_statusLabel->setText(QStringLiteral("[⌛️Waiting] ") + msg);
    m_statusLabel->setVisible(true);
}

void SettingItemWidget::setStatusOK(const QString &msg)
{
    if (!m_statusLabel || !m_statusTimer) {
        return;
    }

    m_statusTimer->stop();

    const QString okStyle = QStringLiteral("QLabel#statusLabel { color: #00FF00; font-weight: bold; font-size: 14px; }");
    if (m_statusStyle != okStyle) {
        m_statusLabel->setStyleSheet(okStyle);
        m_statusStyle = okStyle;
    }

    m_statusLabel->setText(QStringLiteral("[✔OK] ") + msg);
    m_statusLabel->setVisible(true);
    m_statusTimer->start(5000);
}

void SettingItemWidget::setStatusFailed(const QString &msg)
{
    if (!m_statusLabel || !m_statusTimer) {
        return;
    }

    m_statusTimer->stop();

    const QString failedStyle = QStringLiteral("QLabel#statusLabel { color: #FF0000; font-weight: bold; font-size: 14px; }");
    if (m_statusStyle != failedStyle) {
        m_statusLabel->setStyleSheet(failedStyle);
        m_statusStyle = failedStyle;
    }

    m_statusLabel->setText(QStringLiteral("[✘Failed] ") + msg);
    m_statusLabel->setVisible(true);
    m_statusTimer->start(10000);
}

void SettingItemWidget::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);

    QStyleOption option;
    option.initFrom(this);

    QPainter painter(this);
    style()->drawPrimitive(QStyle::PE_Widget, &option, &painter, this);
}

void SettingItemWidget::initUI()
{
    setStyleSheet(QStringLiteral("SettingItemWidget { border-bottom: 1px solid rgba(0, 50, 120, 200); border-radius: 6px; }"));

    m_mainLayout = new QVBoxLayout(this);
    m_mainLayout->setContentsMargins(10, 8, 10, 8);
    m_mainLayout->setSpacing(5);

    m_widgetActions = new QWidget(this);
    m_actionsLayout = new QGridLayout(m_widgetActions);
    m_actionsLayout->setContentsMargins(0, 0, 0, 0);
    m_actionsLayout->setSpacing(10);

    m_titleLabel = new QLabel(m_widgetActions);
    m_titleLabel->setObjectName(QStringLiteral("titleLabel"));
    m_actionsLayout->addWidget(m_titleLabel, 0, 0);

    m_actionsLayout->addItem(new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum), 0, 1);

    m_statusLabel = new QLabel(m_widgetActions);
    m_statusLabel->setObjectName(QStringLiteral("statusLabel"));
    m_statusLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    m_statusLabel->setMinimumWidth(200);
    m_statusLabel->setVisible(false);

    m_statusTimer = new QTimer(this);
    m_statusTimer->setSingleShot(true);
    connect(m_statusTimer, &QTimer::timeout, this, &SettingItemWidget::hideStatusLabel);

    m_mainLayout->addWidget(m_widgetActions);

    m_tipLabel = new QLabel(this);
    m_tipLabel->setObjectName(QStringLiteral("tipLabel"));
    m_tipLabel->setWordWrap(true);
    m_tipLabel->setMinimumWidth(800);
    m_mainLayout->addWidget(m_tipLabel);

    setTitle(QStringLiteral("Setting Item"));
    setTip(QStringLiteral("Please configure the related parameters"));
    setFontSize(14);
}

void SettingItemWidget::hideStatusLabel()
{
    if (m_statusLabel) {
        m_statusLabel->setVisible(false);
    }
}
