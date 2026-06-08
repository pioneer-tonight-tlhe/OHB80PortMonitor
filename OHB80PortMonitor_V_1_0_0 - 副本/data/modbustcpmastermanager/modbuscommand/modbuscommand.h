#ifndef MODBUSCOMMAND_H
#define MODBUSCOMMAND_H

#include <QByteArray>
#include <QString>
#include <QStringList>
#include <QMap>
#include <QMetaType>
#include <QtGlobal>

// ============================================================
// CommandModule - 指令模块枚举
// 不同模块负责发送指令
// ============================================================
enum class CommandModule {
    InitialCommandIssuer,    // 初始指令下发器 - 连接时发送一次
    PeriodicCommandSender,   // 定时指令发送器 - 定时发送
    BusinessCommandIssuer    // 业务指令下发器 - 业务层下发指令
};

// ============================================================
// ModbusFrame - 单帧字段结构（请求帧 / 响应帧共用）
//
// 命名字段供外部直接访问（用于 libmodbus API 调用）：
//   slaveAddr     → modbus_set_slave()
//   functionCode  → 决定调用哪个 libmodbus 函数
//   startAddr     → 寄存器/线圈起始地址
//   count         → 寄存器/线圈数量（XML: RegisterCount / CoilCount）
//   byteCount     → 数据字节数（写多条 / 响应帧使用）
//   registerValue → 数据载荷（XML: RegisterValue / CoilValue / RegisterValues）
//   rawBytes      → 全部字段按 XML 顺序拼接的原始字节序列
// ============================================================
struct ModbusFrame
{
    quint8     slaveAddr    = 0;    // 从机地址
    quint8     functionCode = 0;    // 功能码
    quint16    startAddr    = 0;    // 起始地址（高字节在前）
    quint16    count        = 0;    // 数量（RegisterCount / CoilCount）
    quint8     byteCount    = 0;    // 数据字节数（写多个/响应时有效）
    QByteArray registerValue;       // 数据载荷（XML: RegisterValue / CoilValue / RegisterValues）

    QByteArray rawBytes;            // 完整原始字节序列（字段顺序与 XML 一致）
    QByteArray crc;                 // CRC16 校验值（2字节，用于日志记录）

    bool isValid() const { return functionCode != 0; }
};

// ============================================================
// ModbusCommand - Modbus 指令对象
//
// 存放一条指令的请求帧、期望响应帧，以及运行时控制信息。
// 外部发送指令时：
//   1. modbus_set_slave(ctx, cmd.request.slaveAddr)
//   2. 根据 cmd.request.functionCode 调用对应的 libmodbus 函数
//   3. 参数使用 cmd.request.startAddr / count / registerValue
// ============================================================
class ModbusCommand
{
public:
    // 默认构造函数
    ModbusCommand();

    // 拷贝构造函数
    ModbusCommand(const ModbusCommand& other);

    // 拷贝赋值操作符
    ModbusCommand& operator=(const ModbusCommand& other); 

    // 重置运行时状态（从池中取出后调用）
    void resetState() {
        received = false;
        timedOut = false;
        checksumError = false;
        deviceBusy = false;
        sendCount = 0;
        sentMs   = 0;
        responseMs = 0;
        errorMessage.clear();
        request.crc.clear();
        response.crc.clear();
        // uuid、maxRetryCount、timeoutMs 不重置，属于指令属性
    }

    bool isValid() const { 
        return !id.isEmpty() && request.isValid();
    }
    
    // 是否为业务指令
    bool isBusinessCmd() const {
        return module == CommandModule::BusinessCommandIssuer;
    }
    
    // 方便日志输出的格式化字符串
    QString toLogString() const;
    
    // Convert XML hexadecimal string to byte array
    static QByteArray fromHexString(const QString& hexStr);

    QString     id;        // 指令唯一标识（对应 XML Command id）
    qint64      uuid = 0;   // 实例唯一标识（clone 时生成，用于追踪具体实例）
    ModbusFrame request;   // 请求帧（含命名字段 + rawBytes + crc）
    ModbusFrame response;  // 期望响应帧模板（含命名字段 + rawBytes + crc）

    // ------ 模块和错误信息 ------
    CommandModule module = CommandModule::BusinessCommandIssuer;  // 所属模块
    QString     errorMessage;  // 错误信息（记录发送失败原因）

    // ------ 发送控制参数 ------
    int    maxRetryCount = 3;  // 最大重发次数（不含首次发送）
    int    timeoutMs     = 1000; // 响应超时时间（ms）

    // ------ 运行时控制信息 ------
    bool   received = false;      // 是否已收到从机响应
    bool   timedOut = false;      // 是否已超时
    bool   checksumError = false; // 是否校验错误
    bool   deviceBusy = false;    // 设备繁忙（队列已满，指令被拒绝）
    int    sendCount = 0;         // 发送次数
    qint64 sentMs   = 0;          // 发送时刻（QDateTime::currentMSecsSinceEpoch）
    qint64 responseMs = 0;        // 从机响应时刻（QDateTime::currentMSecsSinceEpoch）
};

Q_DECLARE_METATYPE(ModbusCommand)

#endif // MODBUSCOMMAND_H
