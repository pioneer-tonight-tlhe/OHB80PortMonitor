#include "appconfig.h"
#include "ohbdeviceconfig.h"
#include "userinfoconfig.h"
#include <QCoreApplication>
#include <QDebug>

AppConfig& AppConfig::getInstance()
{
    static AppConfig instance;
    return instance;
}

AppConfig::AppConfig()
    : m_ohbDeviceConfig(nullptr)
    , m_userInfoConfig(nullptr)
{
    initializePaths();
    
    QString configPath = m_configDir + "/app.ini";
    m_settings = new QSettings(configPath, QSettings::IniFormat);
    
    qDebug() << "Config file:" << configPath;
    qDebug() << "Root dir:" << m_rootDir;
    qDebug() << "Executable dir:" << m_executableDir;
    qDebug() << "Config dir:" << m_configDir;
    qDebug() << "Debug log dir:" << m_debugLogDir;
    qDebug() << "User log dir:" << m_userLogDir;
    qDebug() << "Database dir:" << m_databaseDir;
}

void AppConfig::initializePaths()
{
    // 获取可执行文件所在目录
    QString executablePath = QCoreApplication::applicationFilePath();
    m_executableDir = QFileInfo(executablePath).absolutePath();  // bin/x32 或 bin/x64
    
    // config、log、data目录在bin目录下（executableDir的上一级）
    QDir binDir(m_executableDir);
    binDir.cdUp();  // 返回上一级目录，即bin目录
    m_rootDir = binDir.absolutePath();
    
    m_configDir = m_rootDir + "/config";
    m_debugLogDir = m_executableDir + "/log";
    m_userLogDir = m_rootDir + "/log";
    m_databaseDir = m_executableDir + "/databases";
    
    // 确保所有目录存在
    QStringList dirs = {
        m_executableDir,
        m_configDir,
        m_debugLogDir,
        m_userLogDir,
        m_databaseDir
    };
    
    for (const QString& dir : dirs) {
        QDir directory(dir);
        if (!directory.exists()) {
            if (!directory.mkpath(".")) {
                qCritical() << "Failed to create directory:" << dir;
            } else {
                qDebug() << "Created directory:" << dir;
            }
        }
    }
}

QString AppConfig::getAppName() const
{
    return getValue("Application/AppName", "Template").toString();
}

QString AppConfig::getAppVersion() const
{
    return getValue("Application/AppVersion", "1.0.0").toString();
}

QString AppConfig::getOSType() const
{
    return getValue("Application/OSType", "x64").toString();
}

QString AppConfig::getRootDir() const
{
    return m_rootDir;
}

QString AppConfig::getExecutableDir() const
{
    return m_executableDir;
}

QString AppConfig::getConfigDir() const
{
    return m_configDir;
}

QString AppConfig::getDebugLogDir() const
{
    return m_debugLogDir;
}

QString AppConfig::getUserLogDir() const
{
    return m_userLogDir;
}

QString AppConfig::getGraphConfigPath() const
{
    return m_configDir + "/graph_config.xml";
}

QString AppConfig::getOHBDeviceConfigPath() const
{
    return m_configDir + "/ohb_device.ini";
}

QString AppConfig::getUserInfoConfigPath() const
{
    return m_configDir + "/userinfo.ini";
}

QString AppConfig::getModbusConfigPath() const
{
    return m_configDir + "/ModbusTcpMasterConfig.xml";
}

QString AppConfig::getDatabaseDir() const
{
    return m_databaseDir;
}

QString AppConfig::getAlarmLogQueriesSqlPath() const
{
    return m_databaseDir + "/alarm_log_queries.sql";
}

QString AppConfig::getCommunicateLogQueriesSqlPath() const
{
    return m_databaseDir + "/communicate_log_queries.sql";
}

QString AppConfig::getCreateOperationLogSqlPath() const
{
    return m_databaseDir + "/create_operation_log.sql";
}

QString AppConfig::getDeviceParamLogQueriesSqlPath() const
{
    return m_databaseDir + "/device_param_log_queries.sql";
}

QString AppConfig::getOperationLogQueriesSqlPath() const
{
    return m_databaseDir + "/operation_log_queries.sql";
}

OHBDeviceConfig& AppConfig::getOHBDeviceConfig() const
{
    if (!m_ohbDeviceConfig) {
        m_ohbDeviceConfig = &OHBDeviceConfig::getInstance();
    }
    return *m_ohbDeviceConfig;
}

UserInfoConfig& AppConfig::getUserInfoConfig() const
{
    if (!m_userInfoConfig) {
        m_userInfoConfig = &UserInfoConfig::getInstance();
    }
    return *m_userInfoConfig;
}

void AppConfig::reload()
{
    if (m_settings) {
        delete m_settings;
    }
    
    QString configPath = m_configDir + "/app.ini";
    m_settings = new QSettings(configPath, QSettings::IniFormat);
}

bool AppConfig::setValue(const QString& key, const QVariant& value)
{
    if (!m_settings) return false;
    
    m_settings->setValue(key, value);
    m_settings->sync();
    return true;
}

QVariant AppConfig::getValue(const QString& key, const QVariant& defaultValue) const
{
    if (!m_settings) return defaultValue;
    
    return m_settings->value(key, defaultValue);
}
