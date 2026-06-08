#ifndef SQLMAPPER_H
#define SQLMAPPER_H

#include <QString>
#include <QMap>

namespace LogDB {

class SqlMapper
{
public:
    explicit SqlMapper(const QString& sqlFilePath);
    ~SqlMapper();

    // 通过ID获取对应的SQL语句
    QString getSql(const QString& id) const;

    // 检查ID是否存在
    bool contains(const QString& id) const;

    // 获取所有可用的ID列表
    QStringList getAllIds() const;

private:
    // 初始化SQL映射表
    void initializeSqlMap();

    // 从文件加载SQL语句
    bool loadSqlFromFile(const QString& filePath);

    // SQL语句映射表
    QMap<QString, QString> m_sqlMap;
    QString m_sqlFilePath;
};

} // namespace LogDB

#endif // SQLMAPPER_H
