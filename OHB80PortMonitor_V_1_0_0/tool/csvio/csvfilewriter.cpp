#include "csvfilewriter.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTextStream>

bool CsvFileWriter::ensureFileWithHeader(const QString &filePath,
                                         const QStringList &headers,
                                         QString *errorMessage)
{
    if (filePath.trimmed().isEmpty()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("CSV file path is empty");
        }
        return false;
    }

    const QFileInfo fileInfo(filePath);
    QDir dir = fileInfo.dir();
    if (!dir.exists() && !dir.mkpath(QStringLiteral("."))) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Failed to create CSV directory: %1").arg(dir.absolutePath());
        }
        return false;
    }

    if (fileInfo.exists() && fileInfo.size() > 0) {
        if (errorMessage) {
            errorMessage->clear();
        }
        return true;
    }

    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Failed to open CSV file: %1, %2")
                                .arg(filePath, file.errorString());
        }
        return false;
    }

    QTextStream stream(&file);
    stream.setCodec("UTF-8");
    if (!headers.isEmpty()) {
        stream << joinCsvLine(headers) << "\n";
    }
    stream.flush();

    if (file.error() != QFile::NoError) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Failed to write CSV header: %1, %2")
                                .arg(filePath, file.errorString());
        }
        return false;
    }

    if (errorMessage) {
        errorMessage->clear();
    }
    return true;
}

bool CsvFileWriter::appendRow(const QString &filePath,
                              const QStringList &headers,
                              const QStringList &row,
                              QString *errorMessage)
{
    if (!ensureFileWithHeader(filePath, headers, errorMessage)) {
        return false;
    }

    QFile file(filePath);
    if (!file.open(QIODevice::Append | QIODevice::Text)) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Failed to append CSV file: %1, %2")
                                .arg(filePath, file.errorString());
        }
        return false;
    }

    QTextStream stream(&file);
    stream.setCodec("UTF-8");
    stream << joinCsvLine(row) << "\n";
    stream.flush();

    if (file.error() != QFile::NoError) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Failed to append CSV row: %1, %2")
                                .arg(filePath, file.errorString());
        }
        return false;
    }

    if (errorMessage) {
        errorMessage->clear();
    }
    return true;
}

QString CsvFileWriter::joinCsvLine(const QStringList &fields)
{
    QStringList escapedFields;
    escapedFields.reserve(fields.size());
    for (const QString &field : fields) {
        escapedFields << escapeField(field);
    }
    return escapedFields.join(QLatin1Char(','));
}

QString CsvFileWriter::escapeField(const QString &field)
{
    QString escaped = field;
    const bool needsQuotes = escaped.contains(QLatin1Char(','))
                          || escaped.contains(QLatin1Char('"'))
                          || escaped.contains(QLatin1Char('\n'))
                          || escaped.contains(QLatin1Char('\r'));

    escaped.replace(QStringLiteral("\""), QStringLiteral("\"\""));
    return needsQuotes ? QStringLiteral("\"%1\"").arg(escaped) : escaped;
}
