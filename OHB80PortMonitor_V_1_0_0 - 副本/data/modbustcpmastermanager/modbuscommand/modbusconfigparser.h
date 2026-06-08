#ifndef MODBUSCONFIGPARSER_H
#define MODBUSCONFIGPARSER_H

#include "modbuscommand.h"
#include "commandpool.h"
#include <QString>
#include <QStringList>
#include <QQueue>
#include <QList>
#include <QDomElement>

// ============================================================
// CommandGroupConfig - 指令组配置
// 对应 XML 中的 <InitialCommands>、<PeriodicCommands> 或 <BusinessCommands>
// ============================================================
struct CommandGroupConfig
{
    int         interval   = 1000;  // 指令间隔(ms) - InitialCommands/PeriodicCommands 使用
    int         timeout    = 1000;  // 单条指令超时(ms)
    int         retryCount = 0;     // 失败重试次数 - 所有指令组使用
    QStringList refs;               // 本组引用的指令 ID 列表（BusinessCommands 无此字段）
};

// ============================================================
// ModbusConfigParser - Modbus XML 配置文件解析器
//
// 解析 ModbusTcpMasterConfig.xml，产出：
//   - InitialCommands 配置   → initialConfig()
//   - PeriodicCommands 配置  → periodicConfig()
//   - CommandPool（指令格式池）→ pool()
//
// 解析过程由状态机驱动，可通过 state() 实时监控当前阶段
// ============================================================
class ModbusConfigParser
{
public:
    // ============================================================
    // ParseState - 解析状态机枚举
    //
    // 状态流转:
    //   Idle → OpeningFile → ParsingXml → ValidatingRoot
    //        → ParsingInitialCommands → ParsingPeriodicCommands
    //        → ParsingBusinessCommands → ParsingCommandDefinitions → Completed
    //   任意阶段出错 → Failed
    // ============================================================
    enum class ParseState {
        Idle,                       // 初始状态，尚未开始解析
        OpeningFile,                // 正在打开 XML 文件
        ParsingXml,                 // 正在解析 XML DOM 树
        ValidatingRoot,             // 正在验证根节点
        ParsingInitialCommands,     // 正在解析 <InitialCommands>
        ParsingPeriodicCommands,    // 正在解析 <PeriodicCommands>
        ParsingBusinessCommands,    // 正在解析 <BusinessCommands>
        ParsingCommandDefinitions,  // 正在解析 <CommandDefinitions>
        Completed,                  // 解析成功完成
        Failed                      // 解析失败
    };

    // 加载并解析 XML 文件；成功返回 true
    bool parse(const QString& xmlFilePath);

    // 查询当前解析状态
    ParseState    state()        const { return m_state; }

    // 获取失败时的错误信息（仅 Failed 状态有效）
    QString       errorMessage() const { return m_errorMsg; }

    /**
     * @brief 获取初始指令配置
     * @return CommandGroupConfig 包含 interval、timeout、retryCount 和 refs 列表
     * 
     * 此方法返回 XML 文件中 <InitialCommands> 部分的解析配置。返回的结构包含：
     * 
     * @note 此方法必须在成功调用 parse() 之后使用
     * @see initialCommandQueue() 获取实际的指令对象
     */
    CommandGroupConfig  initialConfig()  const { return m_initial; }

    /**
     * @brief 获取定时指令配置
     * @return CommandGroupConfig 包含 interval、timeout、retryCount 和 refs 列表
     * 
     * 此方法返回 XML 文件中 <PeriodicCommands> 部分的解析配置。返回的结构包含：
     * 
     * @note 此方法必须在成功调用 parse() 之后使用
     * @see periodicCommandQueue() 获取实际的指令对象
     */
    CommandGroupConfig  periodicConfig() const { return m_periodic; }

    /**
     * @brief 获取业务指令配置
     * @return CommandGroupConfig 包含 timeout 和 retryCount（业务指令无 interval 和 refs）
     * 
     * 此方法返回 XML 文件中 <BusinessCommands> 部分的解析配置。
     * 业务指令由业务层动态下发，无需预定义指令集，故无 interval 和 refs 字段。
     * 
     * @note 此方法必须在成功调用 parse() 之后使用
     */
    CommandGroupConfig  businessConfig() const { return m_business; }

    // 获取初始下发指令队列（按 refs 顺序从 pool 中克隆）
    QList<ModbusCommand> initialCommandQueue() const;

    // 获取定时发送指令队列（按 refs 顺序从 pool 中克隆）
    QList<ModbusCommand> periodicCommandQueue() const;
    
    /**
     * @brief 获取指令池的拷贝，包含所有解析出的指令模板
     * @return CommandPool 值拷贝，包含所有已注册的指令定义
     * 
     * 此方法返回指令池的副本，该池包含从 XML 文件 <Commands> 部分解析的
     * 所有指令模板。池通过唯一 ID 存储指令定义，并允许克隆它们用于实际执行。
     * 
     * 返回值支持直接拷贝赋值：
     *   CommandPool pool = parser.pool();
     * @note 此方法必须在成功调用 parse() 之后使用
     * @see CommandPool 类文档获取详细 API
     */
    CommandPool  pool() const { return m_pool; }


private:
    // 状态枚举转可读字符串（英文标识符）
    static const char* stateToString(ParseState s);

    // 状态枚举转任务描述（中文，用于日志输出）
    static QString stateDescription(ParseState s);

    // 状态转换：记录日志并更新 m_state
    void transition(ParseState newState);

    // 状态转换到 Failed：记录错误日志并设置 m_errorMsg
    void fail(const QString& reason);

    CommandGroupConfig  m_initial;
    CommandGroupConfig  m_periodic;
    CommandGroupConfig  m_business;
    CommandPool         m_pool;

    ParseState          m_state    = ParseState::Idle;
    QString             m_errorMsg;

    // 解析 <CommandDefinitions> 下的单个 <Command> 节点
    ModbusCommand parseCommandDef(const QDomElement& cmdElem) const;

    // 解析 <request>/<respond>，返回填充好命名字段和 rawBytes 的 ModbusFrame
    ModbusFrame parseFrame(const QDomElement& frameElem) const;

    // 从 <CommandSet> 下提取所有 <Command ref="..."> 的 ref 列表
    QStringList parseCommandSet(const QDomElement& setElem) const;

    // 读取指定元素的整数文本（元素不存在时返回 defaultVal）
    int readInt(const QDomElement& parent,
                const QString& tagName,
                int defaultVal = 0) const;
};

#endif // MODBUSCONFIGPARSER_H
