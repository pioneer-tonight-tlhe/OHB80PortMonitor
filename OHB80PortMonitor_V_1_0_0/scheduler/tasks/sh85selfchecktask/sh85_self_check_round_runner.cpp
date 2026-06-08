#include "sh85_self_check_round_runner.h"

SH85SelfCheckRoundRunner::SH85SelfCheckRoundRunner(QObject* parent)
    : QObject(parent)
{
}

SH85SelfCheckRoundRunner::~SH85SelfCheckRoundRunner()
{
    disconnectAllCheckers();
}

bool SH85SelfCheckRoundRunner::startDevice(const QString& qrcode,
                                           SH85SelfChecker* checker,
                                           QString* errorMessage)
{
    if (!checker) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Self-checker is null");
        }
        return false;
    }

    disconnectDevice(qrcode);

    // checker 可能位于设备线程，统一使用 QueuedConnection 保持跨线程投递安全。
    QList<QMetaObject::Connection> connections;
    connections.append(connect(checker, &SH85SelfChecker::countdownTick,
                               this, &SH85SelfCheckRoundRunner::countdownTick,
                               Qt::QueuedConnection));
    connections.append(connect(checker, &SH85SelfChecker::stateChanged,
                               this, &SH85SelfCheckRoundRunner::stateChanged,
                               Qt::QueuedConnection));
    connections.append(connect(checker, &SH85SelfChecker::finished,
                               this, &SH85SelfCheckRoundRunner::finished,
                               Qt::QueuedConnection));
    connections.append(connect(checker, &SH85SelfChecker::commandCompleted,
                               this, &SH85SelfCheckRoundRunner::commandCompleted,
                               Qt::QueuedConnection));
    connections.append(connect(checker, &SH85SelfChecker::commandRetrying,
                               this, &SH85SelfCheckRoundRunner::commandRetrying,
                               Qt::QueuedConnection));
    connections.append(connect(checker, &SH85SelfChecker::errorOccurred,
                               this, &SH85SelfCheckRoundRunner::errorOccurred,
                               Qt::QueuedConnection));

    m_connectionsByQrcode.insert(qrcode, connections);

    // 若 start() 立即失败，撤销本次临时连接，避免后续收到无效信号。
    if (!checker->start()) {
        disconnectDevice(qrcode);
        if (errorMessage) {
            *errorMessage = QStringLiteral("Checker start failed");
        }
        return false;
    }

    return true;
}

void SH85SelfCheckRoundRunner::disconnectDevice(const QString& qrcode)
{
    const QList<QMetaObject::Connection> connections = m_connectionsByQrcode.take(qrcode);
    for (const QMetaObject::Connection& connection : connections) {
        QObject::disconnect(connection);
    }
}

void SH85SelfCheckRoundRunner::disconnectAllCheckers()
{
    // 这里只断开连接，不直接 stop checker；pending 设备如何收口由 Task3 统一决定。
    for (auto it = m_connectionsByQrcode.constBegin(); it != m_connectionsByQrcode.constEnd(); ++it) {
        for (const QMetaObject::Connection& connection : it.value()) {
            QObject::disconnect(connection);
        }
    }
    m_connectionsByQrcode.clear();
}
