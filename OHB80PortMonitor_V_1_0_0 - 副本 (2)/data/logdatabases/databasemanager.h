#ifndef DATABASEMANAGER_H
#define DATABASEMANAGER_H

#include <QString>
#include <QMutex>
#include "operationlogdbcon.h"
#include "communicatelogdbcon.h"
#include "alarmlogdbcon.h"
#include "deviceparamlogdbcon.h"
#include "vefcsensormonitordbcon.h"

namespace LogDB {

class DatabaseManager
{
public:
    // 获取单例实例
    static DatabaseManager& instance();

    // 初始化数据库
    bool initialize(const QString& databasePath);

    // 清理资源
    void cleanup();

    // 获取OperationLogDBCon指针
    OperationLogDBCon* operationLogCon();

    // 获取CommunicateLogDBCon指针
    CommunicateLogDBCon* communicateLogCon();

    // 获取AlarmLogDBCon指针
    AlarmLogDBCon* alarmLogCon();

    // 获取DeviceParamLogDBCon指针
    DeviceParamLogDBCon* deviceParamLogCon();

    // 获取VEFCSensorMonitorDBCon指针
    VEFCSensorMonitorDBCon* vefcSensorMonitorCon();

    // 禁用拷贝构造和赋值
    DatabaseManager(const DatabaseManager&) = delete;
    DatabaseManager& operator=(const DatabaseManager&) = delete;

private:
    DatabaseManager();
    ~DatabaseManager();

    // 读取SQL文件并创建数据库
    bool createDatabaseFromSqlFile(const QString& sqlFilePath, const QString& dbFilePath);

    QString m_databasePath;
    WriteSqlDBCon* m_writeCon;
    OperationLogDBCon* m_operationLogCon;
    CommunicateLogDBCon* m_communicateLogCon;
    AlarmLogDBCon* m_alarmLogCon;
    DeviceParamLogDBCon* m_deviceParamLogCon;
    VEFCSensorMonitorDBCon* m_vefcSensorMonitorCon;

    static QMutex s_mutex;
    static DatabaseManager* s_instance;
};

} // namespace LogDB

#endif // DATABASEMANAGER_H
