#ifndef CSVFILEWRITER_H
#define CSVFILEWRITER_H

#include <QString>
#include <QStringList>

class CsvFileWriter
{
public:
    static bool ensureFileWithHeader(const QString &filePath,
                                     const QStringList &headers,
                                     QString *errorMessage = nullptr);

    static bool appendRow(const QString &filePath,
                          const QStringList &headers,
                          const QStringList &row,
                          QString *errorMessage = nullptr);

    static QString joinCsvLine(const QStringList &fields);

private:
    static QString escapeField(const QString &field);
};

#endif // CSVFILEWRITER_H
