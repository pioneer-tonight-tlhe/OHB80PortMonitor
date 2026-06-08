#include "loadmoreitemwidget.h"
#include <QPushButton>
#include <QProgressBar>
#include <QLabel>
#include <QFont>

LoadMoreItemWidget::LoadMoreItemWidget(Position pos, QWidget *parent)
    : QWidget(parent), m_pos(pos)
{
    m_layout   = new QVBoxLayout(this);
    m_layout->setContentsMargins(4, 2, 4, 2);

    m_btn = new QPushButton(
        pos == Position::Top
            ? tr("↑ Load Previous")
            : tr("↓ Load Next"), this);
    m_btn->setCursor(Qt::PointingHandCursor);

    m_progress = new QProgressBar(this);
    m_progress->setRange(0, 100);
    m_progress->setFixedHeight(6);
    m_progress->setTextVisible(false);
    m_progress->hide();

    m_label = new QLabel(this);
    m_label->setAlignment(Qt::AlignCenter);
    QFont f = m_label->font();
    f.setPointSize(8);
    m_label->setFont(f);
    m_label->hide();

    m_layout->addWidget(m_btn);
    m_layout->addWidget(m_progress);
    m_layout->addWidget(m_label);
    setLayout(m_layout);

    connect(m_btn, &QPushButton::clicked, this, &LoadMoreItemWidget::clicked);
}

void LoadMoreItemWidget::setIdle()
{
    m_btn->setEnabled(true);
    m_btn->setText(m_pos == Position::Top ? tr("↑ Load Previous") : tr("↓ Load Next"));
    m_progress->hide();
    m_label->hide();
}

void LoadMoreItemWidget::setLoading()
{
    m_btn->setEnabled(false);
    m_btn->setText(tr("Loading..."));
    m_progress->setValue(0);
    m_progress->show();
    m_label->hide();
}

void LoadMoreItemWidget::setProgress(int percent)
{
    m_progress->setValue(percent);
}

void LoadMoreItemWidget::setDone(const QString &timeHint)
{
    m_btn->hide();
    m_progress->hide();
    m_label->setText(timeHint);
    m_label->show();
}
