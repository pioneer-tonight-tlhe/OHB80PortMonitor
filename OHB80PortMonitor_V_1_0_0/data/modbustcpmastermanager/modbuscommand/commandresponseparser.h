#ifndef COMMANDRESPONSEPARSER_H
#define COMMANDRESPONSEPARSER_H

#include "modbuscommand.h"

#include <QMap>
#include <QVariantMap>
#include <functional>

// ============================================================
// CommandResponseParser - 指令响应解析表
//
// 以 QMap<commandId, ParseFunc> 实现统一的响应解析分发。
// 每个 ParseFunc 接收 ModbusCommand（含响应帧），返回 QVariantMap。
//
// 使用方法：
//   QVariantMap data = CommandResponseParser::instance().parse(cmd);
//   if (!data.isEmpty()) { ... }
// ============================================================

class CommandResponseParser
{
public:
    using ParseFunc = std::function<QVariantMap(const ModbusCommand&)>;

    static CommandResponseParser& instance();

    // 注册解析函数（供外部扩展）
    void registerParser(const QString& commandId, ParseFunc func);

    // 是否有对应解析函数
    bool hasParser(const QString& commandId) const;

    // 执行解析，返回 QVariantMap；未注册或解析失败返回空 map
    QVariantMap parse(const ModbusCommand& cmd) const;

    // 解析 1 个版本号寄存器：Byte0 为主版本，Byte1 高4位/低4位分别为次版本/补丁版本。
    static QString parseRegisterVersionString(const QByteArray& payload, bool withLeadingV = false);

private:
    CommandResponseParser();

    // 内置解析函数注册
    void registerBuiltinParsers();

    // ── 各指令静态解析函数 ──────────────────────────────────

    // ReadFoupStatus：9个寄存器，18字节
    // CH_1(0-1) 进气压力/10000   CH_2(2-3)  负压/10000
    // CH_3(4-5) 流量/100         CH_4(6-7)  湿度/100
    // CH_5(8-9) 温度/100         CH_6(10-11) FOUP在位(0/1)
    // CH_7(12-13) 充气秒高16位   CH_8(14-15) 充气秒低16位
    // CH_9(16-17) 设备状态（2byte）：
    //   bit0: VEFC状态（0=OK，1=NG）
    //   bit1: 温湿度传感器（0=OK，1=NG）
    //   bit2: FOUP IN充气30min湿度是否达标（0=OK，1=NG）
    static QVariantMap parseReadFoupStatus(const ModbusCommand& cmd);

    // ReadIdlePurgeEnable：1个寄存器，2字节
    // CH_1(0-1)：0=关闭，1=开启
    static QVariantMap parseReadIdlePurgeEnable(const ModbusCommand& cmd);

    // ReadIdlePurgeStatus：1个寄存器，2字节
    // CH_1(0-1)：0=空闲，1=准备阶段，2=充气，3=充气(间隔)
    static QVariantMap parseReadIdlePurgeStatus(const ModbusCommand& cmd);

    // ReadIdlePurgeWorkingTime：1个寄存器，2字节
    // CH_1(0-1)：计时时长（秒）
    static QVariantMap parseReadIdlePurgeWorkingTime(const ModbusCommand& cmd);

    // ReadIdlePurgeAll：3个寄存器合并帧，6字节
    // CH_1(0-1)：idlePurgeEnabled   CH_2(2-3)：idleState   CH_3(4-5)：idleWorkingTimeSec
    static QVariantMap parseReadIdlePurgeAll(const ModbusCommand& cmd);

    // ReadVEFCPressure：1个寄存器，2字节，原始值 / 10，单位 Kpa
    static QVariantMap parseReadVEFCPressure(const ModbusCommand& cmd);

    // ReadVEFCTemperature：1个寄存器，2字节，原始值 / 100，单位 ℃
    static QVariantMap parseReadVEFCTemperature(const ModbusCommand& cmd);

    // ReadVEFCFlowUnitAndMediumStatus：1个寄存器，hi/lo 分别表示流量单位与介质配置状态。
    static QVariantMap parseReadVEFCFlowUnitAndMediumStatus(const ModbusCommand& cmd);

    // ReadVersion：1个寄存器，2字节。
    // Byte0 表示版本第一段；Byte1 高4位/低4位分别表示第二段/第三段。
    static QVariantMap parseReadVersion(const ModbusCommand& cmd);

    // ReadUIScreenVersion：1个寄存器，2字节。
    // Byte0 表示版本第一段；Byte1 高4位/低4位分别表示第二段/第三段。
    static QVariantMap parseReadUIScreenVersion(const ModbusCommand& cmd);

    QMap<QString, ParseFunc> m_parsers;
};

#endif // COMMANDRESPONSEPARSER_H
