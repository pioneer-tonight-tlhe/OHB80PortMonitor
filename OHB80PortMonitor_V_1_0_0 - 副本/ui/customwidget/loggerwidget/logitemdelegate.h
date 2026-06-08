#pragma once
#include <QStyledItemDelegate>
#include <functional>
#include <QStringList>
#include "itemstyle.h"

class LogPageModel;

// =====================================================================
// LogItemDelegate —— 自定义绘制委托，在 paint() 直接修改调色板，绕过 QSS
// =====================================================================
class LogItemDelegate : public QStyledItemDelegate
{
public:
    using Styler = std::function<void(const QStringList &, ItemStyle &)>;
    explicit LogItemDelegate(QObject *parent = nullptr);

    void setStyler(Styler fn);

    void paint(QPainter *painter, const QStyleOptionViewItem &option,
               const QModelIndex &index) const override;

private:
    Styler m_styler;
};
