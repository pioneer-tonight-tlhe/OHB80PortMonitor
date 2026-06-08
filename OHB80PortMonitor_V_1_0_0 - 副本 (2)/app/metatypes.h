#ifndef METATYPES_H
#define METATYPES_H

#include <QMetaType>

// 前置声明 Page 结构体（用于跨线程信号传递）
struct Page;

class MetaTypes
{
public:
    static void registerTypes();
};

#endif // METATYPES_H
