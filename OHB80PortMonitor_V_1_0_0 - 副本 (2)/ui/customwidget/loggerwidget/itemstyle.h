#pragma once
#include <QColor>
#include <QFont>

// ====================================================================
// ItemStyle —— 单行样式描述，由外部回调负责填写
// ====================================================================
struct ItemStyle {
    QColor foreground;           // 文字颜色
    QColor background;           // 背景颜色
    QFont  font;                 // 字体
    bool   applyForeground = false;
    bool   applyBackground = false;
    bool   applyFont       = false;

    void setForeground(const QColor &c) { foreground = c; applyForeground = true; }
    void setBackground(const QColor &c) { background = c; applyBackground = true; }
    void setFont(const QFont &f)        { font = f;       applyFont       = true; }
};
