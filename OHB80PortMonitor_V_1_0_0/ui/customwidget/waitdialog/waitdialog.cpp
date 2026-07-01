#include "waitdialog.h"

#include <QHideEvent>
#include <QLabel>
#include <QPushButton>
#include <QTimer>
#include <QVBoxLayout>

namespace {

QString formatElapsedTime(qint64 elapsedMs)
{
    const qint64 totalSeconds = elapsedMs / 1000;
    const qint64 hours = totalSeconds / 3600;
    const qint64 minutes = (totalSeconds % 3600) / 60;
    const qint64 seconds = totalSeconds % 60;

    if (hours > 0) {
        return QStringLiteral("%1:%2:%3")
            .arg(hours, 2, 10, QLatin1Char('0'))
            .arg(minutes, 2, 10, QLatin1Char('0'))
            .arg(seconds, 2, 10, QLatin1Char('0'));
    }

    return QStringLiteral("%1:%2")
        .arg(minutes, 2, 10, QLatin1Char('0'))
        .arg(seconds, 2, 10, QLatin1Char('0'));
}

} // namespace

WaitDialog::WaitDialog(QWidget *parent)
    : QDialog(parent)
    , m_mode(Mode::Waiting)
    , m_label(nullptr)
    , m_elapsedLabel(nullptr)
    , m_button(nullptr)
    , m_elapsedTimer(nullptr)
{
    setWindowTitle(tr("Please Wait"));
    setWindowModality(Qt::ApplicationModal);
    setMinimumWidth(280);
    // 移除右上角的"?"帮助按钮
    setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);

    m_label = new QLabel(this);
    m_label->setAlignment(Qt::AlignCenter);
    m_label->setWordWrap(true);

    m_elapsedLabel = new QLabel(this);
    m_elapsedLabel->setAlignment(Qt::AlignCenter);

    m_button = new QPushButton(this);
    m_button->setMinimumWidth(80);

    auto* layout = new QVBoxLayout(this);
    layout->setSpacing(12);
    layout->addWidget(m_label);
    layout->addWidget(m_elapsedLabel);
    layout->addWidget(m_button, 0, Qt::AlignCenter);

    m_elapsedTimer = new QTimer(this);
    m_elapsedTimer->setInterval(1000);
    connect(m_elapsedTimer, &QTimer::timeout, this, &WaitDialog::updateElapsedTime);
    connect(m_button, &QPushButton::clicked, this, &WaitDialog::onButtonClicked);

    setWaiting(QString());
}

void WaitDialog::setWaiting(const QString& message)
{
    m_mode = Mode::Waiting;
    m_label->setText(message);
    m_elapsedTime.restart();
    updateElapsedTime();
    m_elapsedTimer->start();
    m_button->setText(tr("Cancel"));
}

void WaitDialog::setSuccess(const QString& message)
{
    m_mode = Mode::Success;
    m_elapsedTimer->stop();
    updateElapsedTime();
    m_label->setText(message);
    m_button->setText(tr("OK"));
}

void WaitDialog::setFailure(const QString& message)
{
    m_mode = Mode::Failure;
    m_elapsedTimer->stop();
    updateElapsedTime();
    m_label->setText(message);
    m_button->setText(tr("OK"));
}

void WaitDialog::onButtonClicked()
{
    if (m_mode == Mode::Waiting) {
        emit cancelRequested();
        // 不在此处自动关闭：由发起方决定关闭时机（例如先切换为"已取消"再关闭）
    } else {
        accept();
    }
}

void WaitDialog::updateElapsedTime()
{
    m_elapsedLabel->setText(tr("Waiting time: %1").arg(formatElapsedTime(m_elapsedTime.elapsed())));
}

void WaitDialog::hideEvent(QHideEvent* event)
{
    if (m_elapsedTimer) {
        m_elapsedTimer->stop();
    }
    QDialog::hideEvent(event);
}
