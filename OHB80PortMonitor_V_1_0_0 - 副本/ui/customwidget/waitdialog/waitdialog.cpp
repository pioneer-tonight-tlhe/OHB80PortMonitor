#include "waitdialog.h"

#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

WaitDialog::WaitDialog(QWidget *parent)
    : QDialog(parent)
    , m_mode(Mode::Waiting)
    , m_label(nullptr)
    , m_button(nullptr)
{
    setWindowTitle(tr("Please Wait"));
    setWindowModality(Qt::ApplicationModal);
    setMinimumWidth(280);
    // 移除右上角的"?"帮助按钮
    setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);

    m_label = new QLabel(this);
    m_label->setAlignment(Qt::AlignCenter);
    m_label->setWordWrap(true);

    m_button = new QPushButton(this);
    m_button->setMinimumWidth(80);

    auto* layout = new QVBoxLayout(this);
    layout->setSpacing(12);
    layout->addWidget(m_label);
    layout->addWidget(m_button, 0, Qt::AlignCenter);

    connect(m_button, &QPushButton::clicked, this, &WaitDialog::onButtonClicked);

    setWaiting(QString());
}

void WaitDialog::setWaiting(const QString& message)
{
    m_mode = Mode::Waiting;
    m_label->setText(message);
    m_button->setText(tr("Cancel"));
}

void WaitDialog::setSuccess(const QString& message)
{
    m_mode = Mode::Success;
    m_label->setText(message);
    m_button->setText(tr("OK"));
}

void WaitDialog::setFailure(const QString& message)
{
    m_mode = Mode::Failure;
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
