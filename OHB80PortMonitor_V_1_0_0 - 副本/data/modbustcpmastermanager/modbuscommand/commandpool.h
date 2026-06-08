#ifndef COMMANDPOOL_H
#define COMMANDPOOL_H

#include <QMap>
#include <QStringList>
#include "modbuscommand.h"

// ============================================================
// CommandPool - 指令格式池
//
// 存放从 XML 配置文件解析出的指令模板（ModbusCommand）。
// 外部通过 clone(id) 获取指令副本（已重置运行时状态）。
// 数据载荷标签：RegisterValue / CoilValue / RegisterValues
// ============================================================
class CommandPool {
public:
    CommandPool() = default;                              // 默认构造
    CommandPool(const CommandPool& other);                // 拷贝构造
    CommandPool& operator=(const CommandPool& other);     // 拷贝赋值

    void add(const ModbusCommand& tmpl);           // 注册指令模板
    bool contains(const QString& id) const;         // 检查指令是否存在
    ModbusCommand templateCommand(const QString& id) const; // 获取原始指令模板（不生成 uuid）
    ModbusCommand clone(const QString& id) const;  // 克隆指令副本（已重置状态）
    QStringList ids() const;                        // 获取所有指令ID
    int size() const;                               // 获取指令数量
private:
    QMap<QString, ModbusCommand> m_templates;        // 指令模板存储
};

#endif // COMMANDPOOL_H
