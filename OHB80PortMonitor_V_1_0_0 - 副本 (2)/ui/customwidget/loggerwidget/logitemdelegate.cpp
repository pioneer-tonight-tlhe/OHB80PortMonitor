#include "logitemdelegate.h"
#include "logpagemodel.h"
#include <QPainter>
#include <QStyle>

LogItemDelegate::LogItemDelegate(QObject *parent)
    : QStyledItemDelegate(parent) {}

void LogItemDelegate::setStyler(Styler fn)
{
    m_styler = std::move(fn);
}

void LogItemDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option,
                            const QModelIndex &index) const
{
    QStyleOptionViewItem opt = option;
    initStyleOption(&opt, index);

    ItemStyle itemStyle;
    bool hasStyle = false;
    if (m_styler) {
        const LogPageModel *m = qobject_cast<const LogPageModel*>(index.model());
        if (m && index.row() < m->rowCount()) {
            m_styler(m->recordAt(index.row()), itemStyle);
            hasStyle = true;
        }
    }

    painter->save();

    // ---- 背景 ----
    const bool selected = (opt.state & QStyle::State_Selected);
    if (selected) {
        painter->fillRect(opt.rect, QColor(0x30, 0x80, 0xE8));   // 蓝色
    } else if (hasStyle && itemStyle.applyBackground) {
        painter->fillRect(opt.rect, itemStyle.background);
    } else {
        QVariant bg = index.data(Qt::BackgroundRole);
        if (bg.isValid())
            painter->fillRect(opt.rect, bg.value<QColor>());
    }

    // ---- 文字 ----
    QColor textColor;
    if (selected)
        textColor = Qt::white;
    else if (hasStyle && itemStyle.applyForeground)
        textColor = itemStyle.foreground;
    else
        textColor = opt.palette.color(QPalette::Text);

    if (hasStyle && itemStyle.applyFont)
        painter->setFont(itemStyle.font);

    painter->setPen(textColor);
    QRect textRect = opt.rect.adjusted(4, 0, -4, 0);
    painter->drawText(textRect, Qt::AlignLeft | Qt::AlignVCenter, opt.text);

    painter->restore();
}
