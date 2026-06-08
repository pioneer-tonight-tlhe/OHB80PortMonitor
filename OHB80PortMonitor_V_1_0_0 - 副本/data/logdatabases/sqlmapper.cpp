#include "sqlmapper.h"
#include <QFile>
#include <QTextStream>
#include <QRegularExpression>
#include <QDebug>

namespace LogDB {

SqlMapper::SqlMapper(const QString& sqlFilePath)
    : m_sqlFilePath(sqlFilePath)
{
    initializeSqlMap();
}

SqlMapper::~SqlMapper()
{
}

void SqlMapper::initializeSqlMap()
{
    // 从文件加载SQL语句
    if (!loadSqlFromFile(m_sqlFilePath)) {
        qWarning() << "Failed to load SQL file:" << m_sqlFilePath;
    }
}

bool SqlMapper::loadSqlFromFile(const QString& filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return false;
    }

    QTextStream in(&file);
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    in.setEncoding(QStringConverter::Utf8);  // Qt 6: 显式设置UTF-8编码
#else
    in.setCodec("UTF-8");  // Qt 5: 显式设置UTF-8编码
#endif
    QString currentId;
    QString currentSql;

    QRegularExpression idRegex("-- ID=(\\w+)");

    while (!in.atEnd()) {
        QString line = in.readLine();

        // 检查是否是ID行
        QRegularExpressionMatch match = idRegex.match(line);
        if (match.hasMatch()) {
            // 如果之前有SQL语句，先保存
            if (!currentId.isEmpty() && !currentSql.isEmpty()) {
                m_sqlMap[currentId] = currentSql.trimmed();
                qDebug() << "Saved SQL for ID:" << currentId << "Length:" << currentSql.length();
            }

            // 开始新的SQL块
            currentId = match.captured(1);
            currentSql.clear();
            qDebug() << "Found ID:" << currentId;
            continue;
        }

        // 如果当前没有ID，跳过
        if (currentId.isEmpty()) {
            continue;
        }

        QString trimmed = line.trimmed();

        // 跳过注释行（以--开头）
        if (trimmed.startsWith("--")) {
            qDebug() << "Skipping comment:" << trimmed.left(30);
            continue;
        }

        // 跳过空行
        if (trimmed.isEmpty()) {
            qDebug() << "Skipping empty line";
            continue;
        }

        // 收集SQL语句
        qDebug() << "Adding SQL line:" << trimmed;
        currentSql += trimmed + "\n";
    }

    // 保存最后一个SQL语句
    if (!currentId.isEmpty() && !currentSql.isEmpty()) {
        m_sqlMap[currentId] = currentSql.trimmed();
        qDebug() << "Saved last SQL for ID:" << currentId << "Length:" << currentSql.length();
    }

    file.close();
    qDebug() << "Loaded" << m_sqlMap.size() << "SQL statements";
    return !m_sqlMap.isEmpty();
}

QString SqlMapper::getSql(const QString& id) const
{
    return m_sqlMap.value(id, QString());
}

bool SqlMapper::contains(const QString& id) const
{
    return m_sqlMap.contains(id);
}

QStringList SqlMapper::getAllIds() const
{
    return m_sqlMap.keys();
}

} // namespace LogDB
