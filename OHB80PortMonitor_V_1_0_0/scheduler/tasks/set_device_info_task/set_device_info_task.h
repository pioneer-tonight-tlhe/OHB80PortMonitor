#ifndef SET_DEVICE_INFO_TASK_H
#define SET_DEVICE_INFO_TASK_H

#include "../../scheduler_task.h"

#include "modbustcpmastermanager/modbuscommand/modbuscommand.h"

#include <QList>
#include <QMetaObject>
#include <QString>

class SetDeviceInfoTask : public SchedulerTask
{
    Q_OBJECT

public:
    explicit SetDeviceInfoTask(QObject* parent = nullptr);

    void setDeviceInfo(const QString& oldQrCode,
                       const QString& newQrCode,
                       const QString& ip,
                       quint16 port);

    void start() override;
    void stop() override;
    QString taskType() const override { return QStringLiteral("SetDeviceInfoTask"); }

private slots:
    void onWriteQRCodeFinished(ModbusCommand cmd, const QString& masterId);

private:
    bool prepareTask(QString* errorMessage);
    bool submitWriteQRCodeCommand(QString* errorMessage);
    void applyDeviceInfoChange();
    void resolveOldQRCodeAlarms();
    void disconnectWriteQRCodeSignal();
    void writeCommunicateLog(const ModbusCommand& cmd) const;
    QString writeQRCodeFailureReason(const ModbusCommand& cmd) const;
    void finishTask(bool success, const QString& message);
    void logMessage(const QString& message);
    void logError(const QString& message);
    bool validateInput(QString* errorMessage) const;

private:
    QString m_oldQrCode;
    QString m_newQrCode;
    QString m_ip;
    quint16 m_port = 0;
    QString m_oldIp;
    quint16 m_oldPort = 0;
    qint64 m_pendingWriteQRCodeUuid = 0;
    QList<QMetaObject::Connection> m_writeQRCodeConnections;
    bool m_stopped = false;
    bool m_finished = false;
};

#endif // SET_DEVICE_INFO_TASK_H
