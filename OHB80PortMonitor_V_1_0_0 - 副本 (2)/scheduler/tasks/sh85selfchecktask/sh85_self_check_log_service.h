#ifndef SH85_SELF_CHECK_LOG_SERVICE_H
#define SH85_SELF_CHECK_LOG_SERVICE_H

#include <QString>

class ModbusCommand;

// ====================================================================
// SH85SelfCheckLogService - SH85 自检日志服务
//
// 设计目标：
//   1. 将通讯日志落库逻辑从 Task3 调度编排中拆出。
//   2. 后续增加每任务/每设备日志文件时，只扩展该服务，不回改调度代码。
// ====================================================================
class SH85SelfCheckLogService
{
public:
    void writeCommunicateLog(const ModbusCommand& cmd, const QString& masterId) const;
};

#endif // SH85_SELF_CHECK_LOG_SERVICE_H
