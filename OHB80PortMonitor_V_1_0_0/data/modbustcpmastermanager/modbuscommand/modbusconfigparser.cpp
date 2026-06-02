#include "modbusconfigparser.h"
#include <QDomDocument>
#include <QFile>
#include <QDebug>
#include "modbuslogger.h"

// ============================================================
// stateToString - 状态枚举转可读字符串
// ============================================================
const char* ModbusConfigParser::stateToString(ParseState s)
{
    switch (s) {
        case ParseState::Idle:                      return "Idle";
        case ParseState::OpeningFile:               return "1.OpeningFile";
        case ParseState::ParsingXml:                return "2.ParsingXml";
        case ParseState::ValidatingRoot:            return "3.ValidatingRoot";
        case ParseState::ParsingInitialCommands:    return "4.ParsingInitialCommands";
        case ParseState::ParsingPeriodicCommands:   return "5.ParsingPeriodicCommands";
        case ParseState::ParsingBusinessCommands:   return "6.ParsingBusinessCommands";
        case ParseState::ParsingCommandDefinitions: return "7.ParsingCommandDefinitions";
        case ParseState::Completed:                 return "8.Completed";
        case ParseState::Failed:                    return "Failed";
        default:                                    return "Unknown";
    }
}

// ============================================================
// stateDescription - 状态枚举转任务描述（中文）
// ============================================================
QString ModbusConfigParser::stateDescription(ParseState s)
{
    switch (s) {
        case ParseState::Idle:                      return "空闲等待";
        case ParseState::OpeningFile:               return "打开文件";
        case ParseState::ParsingXml:                return "解析XML";
        case ParseState::ValidatingRoot:            return "验证根节点";
        case ParseState::ParsingInitialCommands:    return "解析初始指令组";
        case ParseState::ParsingPeriodicCommands:   return "解析定时指令组";
        case ParseState::ParsingBusinessCommands:   return "解析业务指令组";
        case ParseState::ParsingCommandDefinitions: return "解析指令定义集合";
        case ParseState::Completed:                 return "全部解析";
        case ParseState::Failed:                    return "解析失败";
        default:                                    return "未知操作";
    }
}

// ============================================================
// transition - 状态转换：记录任务开始 / 完成日志
// ============================================================
void ModbusConfigParser::transition(ParseState newState)
{
    // 记录当前阶段完成（Idle 是初始态，不输出“任务完成”）
    if (m_state != ParseState::Idle) {
        qDebug() << QString("ModbusConfigParser: [%1  End] %2任务完成")
                        .arg(stateToString(m_state))
                        .arg(stateDescription(m_state));
        ModbusLogger::systemInfo("ModbusConfigParser", "transition",
            QString("[%1 End] %2任务完成").arg(stateToString(m_state)).arg(stateDescription(m_state)));
    }

    m_state = newState;

    // 记录新阶段开始（Completed 是终态，输出整体完成）
    if (m_state == ParseState::Completed) {
        qDebug() << "ModbusConfigParser: [Completed] 全部解析任务完成";
        ModbusLogger::systemInfo("ModbusConfigParser", "transition", "[Completed] 全部解析任务完成");
    } else {
        qDebug() << QString("ModbusConfigParser: [%1  Start] %2任务开始")
                        .arg(stateToString(m_state))
                        .arg(stateDescription(m_state));
        ModbusLogger::systemInfo("ModbusConfigParser", "transition",
            QString("[%1 Start] %2任务开始").arg(stateToString(m_state)).arg(stateDescription(m_state)));
    }
}

// ============================================================
// fail - 转换到 Failed 状态并记录错误
// ============================================================
void ModbusConfigParser::fail(const QString& reason)
{
    m_errorMsg = reason;
    qDebug() << "ModbusConfigParser: [FAILED]" << reason;
    ModbusLogger::systemError("ModbusConfigParser", "fail",
        QString("解析失败，阶段=%1，原因=%2").arg(stateToString(m_state)).arg(reason));
    m_state = ParseState::Failed;
}

// ============================================================
// parse - 加载并解析 XML 文件（状态机驱动）
// ============================================================
bool ModbusConfigParser::parse(const QString& xmlFilePath)
{
    m_errorMsg.clear();

    // ---- 阶段1: 打开文件 ----
    transition(ParseState::OpeningFile);

    QFile file(xmlFilePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        fail(QString("无法打开配置文件: %1").arg(xmlFilePath));
        return false;
    }

    // ---- 阶段2: 解析 XML DOM ----
    transition(ParseState::ParsingXml);

    QDomDocument doc;
    QString errMsg;
    int errLine = 0, errCol = 0;
    if (!doc.setContent(&file, &errMsg, &errLine, &errCol)) {
        fail(QString("XML解析错误 第%1行第%2列: %3")
                 .arg(errLine).arg(errCol).arg(errMsg));
        file.close();
        return false;
    }
    file.close();

    // ---- 阶段3: 验证根节点 ----
    transition(ParseState::ValidatingRoot);

    const QDomElement root = doc.documentElement(); // <ModbusTcpMasterConfig>
    if (root.isNull()) {
        fail("XML根节点为空，文件可能损坏或格式错误");
        return false;
    }

    // ---- 阶段4: 解析 <InitialCommands> ----
    transition(ParseState::ParsingInitialCommands);

    const QDomElement initElem = root.firstChildElement("InitialCommands");
    if (!initElem.isNull()) {
        m_initial.interval   = readInt(initElem, "CommandInterval", 100);
        m_initial.timeout    = readInt(initElem, "CommandTimeout",  100);
        m_initial.retryCount = readInt(initElem, "RetryCount",      0);
        m_initial.refs       = parseCommandSet(initElem.firstChildElement("CommandSet"));
        qDebug() << "ModbusConfigParser: InitialCommands 已解析，指令数:"
                 << m_initial.refs.size();
        ModbusLogger::systemInfo("ModbusConfigParser", "parse",
            QString("InitialCommands 已解析，指令数=%1").arg(m_initial.refs.size()));
    } else {
        qDebug() << "ModbusConfigParser: <InitialCommands> 节点不存在，跳过";
        ModbusLogger::systemInfo("ModbusConfigParser", "parse", "<InitialCommands> 节点不存在，跳过");
    }

    // ---- 阶段5: 解析 <PeriodicCommands> ----
    transition(ParseState::ParsingPeriodicCommands);

    const QDomElement periodicElem = root.firstChildElement("PeriodicCommands");
    if (!periodicElem.isNull()) {
        m_periodic.interval = readInt(periodicElem, "CommandInterval", 1000);
        m_periodic.timeout  = readInt(periodicElem, "CommandTimeout",  1000);
        m_periodic.retryCount = readInt(periodicElem, "RetryCount", 3);
        m_periodic.refs     = parseCommandSet(periodicElem.firstChildElement("CommandSet"));
        qDebug() << "ModbusConfigParser: PeriodicCommands 已解析，指令数:"
                 << m_periodic.refs.size();
        ModbusLogger::systemInfo("ModbusConfigParser", "parse",
            QString("PeriodicCommands 已解析，指令数=%1").arg(m_periodic.refs.size()));
    } else {
        qDebug() << "ModbusConfigParser: <PeriodicCommands> 节点不存在，跳过";
        ModbusLogger::systemInfo("ModbusConfigParser", "parse", "<PeriodicCommands> 节点不存在，跳过");
    }

    // ---- 阶段6: 解析 <BusinessCommands> ----
    transition(ParseState::ParsingBusinessCommands);

    const QDomElement businessElem = root.firstChildElement("BusinessCommands");
    if (!businessElem.isNull()) {
        m_business.timeout    = readInt(businessElem, "CommandTimeout", 1000);
        m_business.retryCount = readInt(businessElem, "RetryCount",      3);
        qDebug() << "ModbusConfigParser: BusinessCommands 已解析";
        ModbusLogger::systemInfo("ModbusConfigParser", "parse", "BusinessCommands 已解析");
    } else {
        qDebug() << "ModbusConfigParser: <BusinessCommands> 节点不存在，跳过";
        ModbusLogger::systemInfo("ModbusConfigParser", "parse", "<BusinessCommands> 节点不存在，跳过");
    }

    // ---- 阶段7: 解析 <CommandDefinitions> ----
    transition(ParseState::ParsingCommandDefinitions);

    const QDomElement defsElem = root.firstChildElement("CommandDefinitions");
    if (!defsElem.isNull()) {
        QDomElement cmdElem = defsElem.firstChildElement("Command");
        while (!cmdElem.isNull()) {
            ModbusCommand cmd = parseCommandDef(cmdElem);
            if (!cmd.id.isEmpty()) {
                cmd.timeoutMs = m_business.timeout;
                cmd.maxRetryCount = m_business.retryCount;
                cmd.module = CommandModule::BusinessCommandIssuer;
                m_pool.add(cmd);
                qDebug() << "ModbusConfigParser: 已注册指令模板 -" << cmd.id
                         << "请求帧长度:" << cmd.request.rawBytes.size()
                         << "响应模板长度:" << cmd.response.rawBytes.size();
                ModbusLogger::systemDebug("ModbusConfigParser", "parse",
                    QString("已注册指令模板 id=%1 请求帧长度=%2 响应模板长度=%3")
                        .arg(cmd.id).arg(cmd.request.rawBytes.size()).arg(cmd.response.rawBytes.size()));
            } else {
                ModbusLogger::systemWarn("ModbusConfigParser", "parse", "跳过一条 id 为空的 <Command> 节点");
            }
            cmdElem = cmdElem.nextSiblingElement("Command");
        }
        qDebug() << "ModbusConfigParser: CommandPool 共加载" << m_pool.size() << "条指令";
        ModbusLogger::systemInfo("ModbusConfigParser", "parse",
            QString("CommandPool 共加载 %1 条指令").arg(m_pool.size()));
    } else {
        qDebug() << "ModbusConfigParser: <CommandDefinitions> 节点不存在，跳过";
        ModbusLogger::systemInfo("ModbusConfigParser", "parse", "<CommandDefinitions> 节点不存在，跳过");
    }

    // ---- 阶段7: 完成 ----
    transition(ParseState::Completed);
    return true;
}

// ============================================================
// parseCommandDef - 解析单条 <Command> 节点
// ============================================================
ModbusCommand ModbusConfigParser::parseCommandDef(const QDomElement& cmdElem) const
{
    ModbusCommand cmd;
    cmd.id       = cmdElem.attribute("id");
    if (cmd.id.isEmpty()) {
        ModbusLogger::systemWarn("ModbusConfigParser", "parseCommandDef",
            "遇到缺少 id 属性的 <Command> 节点");
    }
    cmd.request  = parseFrame(cmdElem.firstChildElement("request"));
    cmd.response = parseFrame(cmdElem.firstChildElement("respond"));
    ModbusLogger::systemDebug("ModbusConfigParser", "parseCommandDef",
        QString("解析完成 id=%1 请求帧=%2字节 响应模板=%3字节")
            .arg(cmd.id).arg(cmd.request.rawBytes.size()).arg(cmd.response.rawBytes.size()));
    return cmd;
}

// ============================================================
// parseFrame - 解析 <request>/<respond> 帧节点
// 每个子元素的标签名映射到 ModbusFrame 命名字段，
// 同时将字节值按顺序追加到 rawBytes
// ============================================================
ModbusFrame ModbusConfigParser::parseFrame(const QDomElement& frameElem) const
{
    ModbusFrame frame;
    if (frameElem.isNull()) {
        ModbusLogger::systemDebug("ModbusConfigParser", "parseFrame", "帧节点为空，返回空帧");
        return frame;
    }

    QDomElement field = frameElem.firstChildElement();
    while (!field.isNull()) {
        const QString tag  = field.tagName();
        const QString text = field.text().trimmed();

        if (!text.isEmpty()) {
            const QByteArray bytes = ModbusCommand::fromHexString(text);
            frame.rawBytes += bytes;

            // 按标签名映射到 ModbusFrame 命名字段
            if (tag == "SlaveAddr") {
                frame.slaveAddr = bytes.isEmpty() ? 0 : static_cast<quint8>(bytes[0]);

            } else if (tag == "Function") {
                frame.functionCode = bytes.isEmpty() ? 0 : static_cast<quint8>(bytes[0]);

            } else if (tag == "StartAddr") {
                if (bytes.size() >= 2)
                    frame.startAddr = (static_cast<quint16>(static_cast<quint8>(bytes[0])) << 8)
                                    |  static_cast<quint16>(static_cast<quint8>(bytes[1]));

            } else if (tag == "RegisterCount" || tag == "CoilCount") {
                if (bytes.size() >= 2)
                    frame.count = (static_cast<quint16>(static_cast<quint8>(bytes[0])) << 8)
                                |  static_cast<quint16>(static_cast<quint8>(bytes[1]));

            } else if (tag == "ByteCount") {
                frame.byteCount = bytes.isEmpty() ? 0 : static_cast<quint8>(bytes[0]);

            } else if (tag == "RegisterValue"  ||
                       tag == "RegisterValues" ||
                       tag == "CoilValues"     ||
                       tag == "CoilValue") {
                if (frame.functionCode == 0x06) { // Write Single Register
                    // For function 0x06, RegisterValue is the register value to write
                    if (bytes.size() >= 2) {
                        frame.registerValue = bytes.mid(0, 2);
                    }
                } else {
                    frame.registerValue = bytes;
                }
            }
            // 其他字段（如注释字段）只追加到 rawBytes，不映射命名成员
        }

        field = field.nextSiblingElement();
    }
    return frame;
}

// ============================================================
// parseCommandSet - 从 <CommandSet> 提取所有 ref 列表
// ============================================================
QStringList ModbusConfigParser::parseCommandSet(const QDomElement& setElem) const
{
    QStringList refs;
    if (setElem.isNull()) {
        ModbusLogger::systemDebug("ModbusConfigParser", "parseCommandSet",
            "<CommandSet> 节点为空，返回空列表");
        return refs;
    }

    QDomElement cmd = setElem.firstChildElement("Command");
    while (!cmd.isNull()) {
        const QString ref = cmd.attribute("ref");
        if (!ref.isEmpty()) {
            refs.append(ref);
        }
        cmd = cmd.nextSiblingElement("Command");
    }
    ModbusLogger::systemDebug("ModbusConfigParser", "parseCommandSet",
        QString("提取 ref 列表完成，共 %1 条").arg(refs.size()));
    return refs;
}

// ============================================================
// readInt - 读取子元素的整数值
// ============================================================
int ModbusConfigParser::readInt(const QDomElement& parent,
                                const QString& tagName,
                                int defaultVal) const
{
    const QDomElement elem = parent.firstChildElement(tagName);
    if (elem.isNull()) {
        ModbusLogger::systemDebug("ModbusConfigParser", "readInt",
            QString("元素 <%1> 不存在，使用默认值=%2").arg(tagName).arg(defaultVal));
        return defaultVal;
    }
    bool ok = false;
    const int val = elem.text().trimmed().toInt(&ok);
    if (!ok) {
        ModbusLogger::systemWarn("ModbusConfigParser", "readInt",
            QString("元素 <%1> 值 '%2' 无法转换为整数，使用默认值=%3")
                .arg(tagName).arg(elem.text().trimmed()).arg(defaultVal));
    }
    return ok ? val : defaultVal;
}

// ============================================================
// initialCommandQueue - 按 refs 顺序从 pool 中克隆初始指令队列
// ============================================================
QList<ModbusCommand> ModbusConfigParser::initialCommandQueue() const
{
    QList<ModbusCommand> list;
    ModbusLogger::systemInfo("ModbusConfigParser", "initialCommandQueue",
        QString("开始构建初始指令队列，共 %1 个引用").arg(m_initial.refs.size()));
    for (const QString& ref : m_initial.refs) {
        if (m_pool.contains(ref)) {
            ModbusCommand cmd = m_pool.clone(ref);
            cmd.module = CommandModule::InitialCommandIssuer;
            cmd.maxRetryCount = m_initial.retryCount;
            cmd.timeoutMs = m_initial.timeout;
            list.append(cmd);
            ModbusLogger::systemDebug("ModbusConfigParser", "initialCommandQueue",
                QString("已克隆初始指令 ref=%1").arg(ref));
        } else {
            qDebug() << "ModbusConfigParser: 初始指令引用未找到 -" << ref;
            ModbusLogger::systemWarn("ModbusConfigParser", "initialCommandQueue",
                QString("初始指令引用未找到 ref=%1").arg(ref));
        }
    }
    ModbusLogger::systemInfo("ModbusConfigParser", "initialCommandQueue",
        QString("初始指令队列构建完成，共 %1 条").arg(list.size()));
    return list;
}

// ============================================================
// periodicCommandQueue - 按 refs 顺序从 pool 中克隆定时发送指令队列
// ============================================================
QList<ModbusCommand> ModbusConfigParser::periodicCommandQueue() const
{
    QList<ModbusCommand> list;
    ModbusLogger::systemInfo("ModbusConfigParser", "periodicCommandQueue",
        QString("开始构建定时指令队列，共 %1 个引用").arg(m_periodic.refs.size()));
    for (const QString& ref : m_periodic.refs) {
        if (m_pool.contains(ref)) {
            ModbusCommand cmd = m_pool.clone(ref);
            cmd.module = CommandModule::PeriodicCommandSender;
            cmd.maxRetryCount = m_periodic.retryCount;
            cmd.timeoutMs = m_periodic.timeout;
            list.append(cmd);
            ModbusLogger::systemDebug("ModbusConfigParser", "periodicCommandQueue",
                QString("已克隆定时指令 ref=%1").arg(ref));
        } else {
            qDebug() << "ModbusConfigParser: 定时指令引用未找到 -" << ref;
            ModbusLogger::systemWarn("ModbusConfigParser", "periodicCommandQueue",
                QString("定时指令引用未找到 ref=%1").arg(ref));
        }
    }
    ModbusLogger::systemInfo("ModbusConfigParser", "periodicCommandQueue",
        QString("定时指令队列构建完成，共 %1 条").arg(list.size()));
    return list;
}
