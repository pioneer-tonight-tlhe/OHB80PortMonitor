#ifndef SH85_SELF_CHECK_ROUND_RUNNER_H
#define SH85_SELF_CHECK_ROUND_RUNNER_H

#include "modbustcpmastermanager/modbuscommand/modbuscommand.h"
#include "modbustcpmastermanager/modbustcpmaster/sh85selfchecker.h"

#include <QHash>
#include <QList>
#include <QMetaObject>
#include <QObject>
#include <QString>

// ====================================================================
// SH85SelfCheckRoundRunner - SH85 自检轮次执行器
//
// 设计目标：
//   1. 统一管理 checker 信号连接、启动和断开。
//   2. 只负责中继 checker 事件，不判断业务结果，不写轮次上下文。
//   3. 让 Task3 不再直接维护大量 QMetaObject::Connection。
// ====================================================================
class SH85SelfCheckRoundRunner : public QObject
{
    Q_OBJECT

public:
    explicit SH85SelfCheckRoundRunner(QObject* parent = nullptr);
    ~SH85SelfCheckRoundRunner() override;

    // 启动设备自检：先连接信号再 start()，避免遗漏早期 checker 信号。
    bool startDevice(const QString& qrcode,
                     SH85SelfChecker* checker,
                     QString* errorMessage = nullptr);

    // 只断开信号连接；是否取消 checker 流程由上层 Task 决定。
    void disconnectDevice(const QString& qrcode);
    void disconnectAllCheckers();

signals:
    void countdownTick(int remainingSeconds, const QString& masterId);
    void stateChanged(SH85SelfChecker::State state, const QString& masterId);
    void finished(bool success,
                  SH85SelfChecker::Result result,
                  const QString& message,
                  const QString& masterId,
                  double minimumHumidity);
    void commandCompleted(ModbusCommand cmd, const QString& masterId);
    void commandRetrying(ModbusCommand cmd, const QString& masterId);
    void errorOccurred(SH85SelfChecker::Result result,
                       const QString& message,
                       const QString& masterId);

private:
    QHash<QString, QList<QMetaObject::Connection>> m_connectionsByQrcode;
};

#endif // SH85_SELF_CHECK_ROUND_RUNNER_H
