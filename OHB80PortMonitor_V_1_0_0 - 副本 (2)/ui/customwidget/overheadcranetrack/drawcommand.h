#ifndef DRAWCOMMAND_H
#define DRAWCOMMAND_H

#include <QPointF>

namespace Graph {

// 绘制指令结构
struct DrawCommand {
    enum Type { Line, SCurve, ReverseSCurve, LeftArc, RightArc, LeftArc2, RightArc2 };
    Type type;
    QPointF from;
    QPointF to;
    // 可选：贝塞尔曲线控制点
    QPointF control1;
    QPointF control2;
    // 可选：半圆弧直径（来自配置的Size参数）
    double arcDiameter = 0.0;
};

} // namespace Graph

#endif // DRAWCOMMAND_H
