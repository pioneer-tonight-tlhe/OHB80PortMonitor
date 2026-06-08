#include "databasemanager.h"
#include <QFile>
#include <QTextStream>
#include <QMutexLocker>
#include <QDebug>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>

namespace LogDB {

QMutex DatabaseManager::s_mutex;
DatabaseManager* DatabaseManager::s_instance = nullptr;

DatabaseManager& DatabaseManager::instance()
{
    QMutexLocker locker(&s_mutex);
    if (!s_instance) {
        s_instance = new DatabaseManager();
    }
    return *s_instance;
}

DatabaseManager::DatabaseManager()
    : m_writeCon(nullptr)
    , m_operationLogCon(nullptr)
    , m_communicateLogCon(nullptr)
    , m_alarmLogCon(nullptr)
    , m_deviceParamLogCon(nullptr)
    , m_vefcSensorMonitorCon(nullptr)
{
}

DatabaseManager::~DatabaseManager()
{
    cleanup();
}

bool DatabaseManager::initialize(const QString& databasePath)
{
    m_databasePath = databasePath;

    QString sqlFilePath = QString("%1/create_operation_log.sql").arg(m_databasePath);
    QString dbFilePath = QString("%1/logdb.db").arg(m_databasePath);

    if (!createDatabaseFromSqlFile(sqlFilePath, dbFilePath)) {
        qWarning() << "Failed to initialize database";
        return false;
    }

    // 创建写入连接
    m_writeCon = new WriteSqlDBCon(m_databasePath);
    m_writeCon->initialize();

    // 创建操作日志连接，传入外部写入连接
    m_operationLogCon = new OperationLogDBCon(m_databasePath, m_writeCon, nullptr);
    m_operationLogCon->initialize();

    // 创建通讯日志连接，传入外部写入连接
    m_communicateLogCon = new CommunicateLogDBCon(m_databasePath, m_writeCon, nullptr);
    m_communicateLogCon->initialize();

    // 创建警报日志连接，传入外部写入连接
    m_alarmLogCon = new AlarmLogDBCon(m_databasePath, m_writeCon, nullptr);
    m_alarmLogCon->initialize();

    // 创建设备参数日志连接，传入外部写入连接
    m_deviceParamLogCon = new DeviceParamLogDBCon(m_databasePath, m_writeCon, nullptr);
    m_deviceParamLogCon->initialize();

    // 创建 VEFC 传感器监控连接，传入外部写入连接
    m_vefcSensorMonitorCon = new VEFCSensorMonitorDBCon(m_databasePath, m_writeCon, nullptr);
    m_vefcSensorMonitorCon->initialize();

    return true;
}

void DatabaseManager::cleanup()
{
    if (m_vefcSensorMonitorCon) {
        m_vefcSensorMonitorCon->cleanup();
        delete m_vefcSensorMonitorCon;
        m_vefcSensorMonitorCon = nullptr;
    }

    if (m_deviceParamLogCon) {
        m_deviceParamLogCon->cleanup();
        delete m_deviceParamLogCon;
        m_deviceParamLogCon = nullptr;
    }

    if (m_alarmLogCon) {
        m_alarmLogCon->cleanup();
        delete m_alarmLogCon;
        m_alarmLogCon = nullptr;
    }

    if (m_communicateLogCon) {
        m_communicateLogCon->cleanup();
        delete m_communicateLogCon;
        m_communicateLogCon = nullptr;
    }

    if (m_operationLogCon) {
        m_operationLogCon->cleanup();
        delete m_operationLogCon;
        m_operationLogCon = nullptr;
    }

    if (m_writeCon) {
        m_writeCon->cleanup();
        delete m_writeCon;
        m_writeCon = nullptr;
    }
}

OperationLogDBCon* DatabaseManager::operationLogCon()
{
    return m_operationLogCon;
}

CommunicateLogDBCon* DatabaseManager::communicateLogCon()
{
    return m_communicateLogCon;
}

AlarmLogDBCon* DatabaseManager::alarmLogCon()
{
    return m_alarmLogCon;
}

DeviceParamLogDBCon* DatabaseManager::deviceParamLogCon()
{
    return m_deviceParamLogCon;
}

VEFCSensorMonitorDBCon* DatabaseManager::vefcSensorMonitorCon()
{
    return m_vefcSensorMonitorCon;
}

bool DatabaseManager::createDatabaseFromSqlFile(const QString& sqlFilePath, const QString& dbFilePath)
{
    QFile sqlFile(sqlFilePath);
    if (!sqlFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qWarning() << "Failed to open SQL file:" << sqlFilePath;
        return false;
    }

    QTextStream stream(&sqlFile);
    QString content = stream.readAll();
    sqlFile.close();

    // 移除整行注释和空行；保留行尾换行，避免行内注释吞掉后续字段
    QStringList lines = content.split('\n');
    QStringList cleanedLines;
    for (const QString& line : lines) {
        QString trimmed = line.trimmed();
        if (!trimmed.isEmpty() && !trimmed.startsWith("--")) {
            cleanedLines.append(trimmed);
        }
    }
    QString cleanedSql = cleanedLines.join('\n');

    // 执行SQL语句
    QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE", "InitConnection");
    db.setDatabaseName(dbFilePath);
    if (!db.open()) {
        qWarning() << "Failed to open database:" << db.lastError().text();
        return false;
    }

    QSqlQuery query(db);
    QStringList statements = cleanedSql.split(';', Qt::SkipEmptyParts);
    int successCount = 0;
    for (const QString& statement : statements) {
        if (!statement.trimmed().isEmpty()) {
            if (query.exec(statement)) {
                successCount++;
            } else {
                qWarning() << "Failed to execute SQL:" << statement << query.lastError().text();
            }
        }
    }

    db.close();
    QSqlDatabase::removeDatabase("InitConnection");

    qDebug() << "Database created successfully from SQL file:" << sqlFilePath;
    qDebug() << "Executed" << successCount << "SQL statements";

    return successCount > 0;
}

} // namespace LogDB
