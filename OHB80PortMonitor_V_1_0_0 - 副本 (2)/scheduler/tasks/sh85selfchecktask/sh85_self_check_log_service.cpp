#include "sh85_self_check_log_service.h"

#include "logdatabases/communicatelogdb/communicatelogdbcon.h"
#include "logdatabases/databasemanager.h"
#include "modbustcpmastermanager/modbuscommand/commandresponseparser.h"
#include "modbustcpmastermanager/modbuscommand/modbuscommand.h"
#include "usermanager/usermanager.h"

#include <QDateTime>
#include <QStringList>
#include <QVariantMap>
#include <QtGlobal>

void SH85SelfCheckLogService::writeCommunicateLog(const ModbusCommand& cmd,
                                                  const QString& masterId) const
{
    const QString sentTimeStr = cmd.sentMs > 0
        ? QDateTime::fromMSecsSinceEpoch(cmd.sentMs).toString(QStringLiteral("yyyy-MM-dd HH:mm:ss.zzz"))
        : QStringLiteral("-");

    // 保持 Task2 写 communicate_log 时使用的执行状态语义。
    int execStatus = 3;
    if (cmd.received) {
        execStatus = 0;
    } else if (cmd.timedOut) {
        execStatus = 1;
    } else if (cmd.sendCount > 1) {
        execStatus = 2;
    }

    const int retryCount = qMax(0, cmd.sendCount - 1);
    QString description;
    if (execStatus != 0) {
        description = cmd.errorMessage;
    } else {
        // 成功响应解析为 key=value 描述，便于操作人员查看通讯明细。
        const QVariantMap parsedData = CommandResponseParser::instance().parse(cmd);
        if (!parsedData.isEmpty()) {
            QStringList parts;
            for (auto it = parsedData.constBegin(); it != parsedData.constEnd(); ++it) {
                parts << QStringLiteral("%1=%2").arg(it.key(), it.value().toString());
            }
            description = parts.join(QStringLiteral(", "));
        }
    }

    if (description.isEmpty()) {
        description = QStringLiteral("OK");
    }

    if (LogDB::CommunicateLogDBCon* db = LogDB::DatabaseManager::instance().communicateLogCon()) {
        const QString respTimeStr = cmd.responseMs > 0
            ? QDateTime::fromMSecsSinceEpoch(cmd.responseMs).toString(QStringLiteral("yyyy-MM-dd HH:mm:ss.zzz"))
            : QString();

        db->insertRecord(sentTimeStr,
                         respTimeStr,
                         cmd.id,
                         masterId,
                         execStatus,
                         retryCount,
                         cmd.request.rawBytes,
                         cmd.response.rawBytes,
                         description,
                         UserPermission::Engineer);
    }
}
