#include "commandpool.h"

namespace {
qint64 g_commandUuidCounter = 1;
}

// ============================================================
// CommandPool - 指令格式池
// 存放从配置文件解析出的指令模板；外部通过 clone() 取用副本
// ============================================================

// 拷贝构造：深拷贝所有指令模板
CommandPool::CommandPool(const CommandPool& other)
    : m_templates(other.m_templates)
{
}

// 拷贝赋值：深拷贝所有指令模板，支持链式赋值
CommandPool& CommandPool::operator=(const CommandPool& other)
{
    if (this != &other) {
        m_templates = other.m_templates;
    }
    return *this;
}

// 注册一条指令模板
void CommandPool::add(const ModbusCommand& tmpl)
{
    if (!tmpl.id.isEmpty() && tmpl.request.isValid()) {
        m_templates[tmpl.id] = tmpl;
    }
}

// 判断指令是否存在
bool CommandPool::contains(const QString& id) const
{
    return m_templates.contains(id);
}

ModbusCommand CommandPool::templateCommand(const QString& id) const
{
    return m_templates.value(id);
}

// 从池中拷贝一份指令副本（已重置运行时状态）
ModbusCommand CommandPool::clone(const QString& id) const
{
    ModbusCommand cmd;
    if (m_templates.contains(id)) {
        cmd = m_templates[id];
        cmd.resetState();  // 重置运行时状态
        cmd.uuid = g_commandUuidCounter++;  // 生成递增 uuid
    }
    return cmd;
}

// 返回所有已注册的指令 ID 列表
QStringList CommandPool::ids() const
{
    return m_templates.keys();
}

// 池中指令数量
int CommandPool::size() const
{
    return m_templates.size();
}
