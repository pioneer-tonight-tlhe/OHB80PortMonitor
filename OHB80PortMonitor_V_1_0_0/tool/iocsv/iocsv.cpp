/*******************************************************************************************
 * @file iocsv.cpp
 * @author Simon <工号：13> 2026-07-18
 *
 * @brief 实现 IOCSV 的 CSV 文件读写和记录解析功能。
 *******************************************************************************************/
#include "iocsv.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTextStream>

IOCSV::IOCSV(const QString &filePath)
    : m_filePath(filePath)
{
}

IOCSV::~IOCSV()
{
    close();
}

bool IOCSV::open(const QString &filePath)
{
    if (!filePath.isEmpty()) {
        m_filePath = filePath;
    }

    close();
    m_lastError.clear();

    if (m_filePath.trimmed().isEmpty()) {
        setError(QStringLiteral("CSV file path is empty"));
        return false;
    }

    const QFileInfo fileInfo(m_filePath);
    const QDir parentDirectory = fileInfo.absoluteDir();
    if (!parentDirectory.exists()
        && !QDir().mkpath(parentDirectory.absolutePath())) {
        setError(QString("failed to create CSV directory: %1")
                     .arg(parentDirectory.absolutePath()));
        return false;
    }

    m_file = new QFile(m_filePath);
    if (!m_file->open(QIODevice::ReadWrite | QIODevice::Text)) {
        setError(QString("failed to open CSV file '%1': %2")
                     .arg(m_filePath, m_file->errorString()));
        delete m_file;
        m_file = nullptr;
        return false;
    }

    return true;
}

void IOCSV::close()
{
    if (!m_file) {
        return;
    }

    if (m_file->isOpen()) {
        m_file->close();
    }
    delete m_file;
    m_file = nullptr;
}

bool IOCSV::appendRow(const QStringList &fields)
{
    if (!m_file || !m_file->isOpen()) {
        setError(QStringLiteral("CSV file is not open"));
        return false;
    }

    if (!m_file->seek(m_file->size())) {
        setError(QString("failed to seek CSV file end: %1")
                     .arg(m_file->errorString()));
        return false;
    }

    QTextStream stream(m_file);
    stream.setCodec("UTF-8");
    stream << encodeRow(fields) << QLatin1Char('\n');
    stream.flush();

    if (stream.status() != QTextStream::Ok) {
        setError(QStringLiteral("failed to append CSV record"));
        return false;
    }

    return true;
}

QStringList IOCSV::readRow(int rowIndex) const
{
    if (rowIndex < 0) {
        setError(QStringLiteral("CSV row index must not be negative"));
        return {};
    }

    if (m_filePath.trimmed().isEmpty()) {
        setError(QStringLiteral("CSV file path is empty"));
        return {};
    }

    QFile file(m_filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        setError(QString("failed to read CSV file '%1': %2")
                     .arg(m_filePath, file.errorString()));
        return {};
    }

    QTextStream stream(&file);
    stream.setCodec("UTF-8");
    const QVector<QStringList> rows = decodeRows(stream.readAll());
    if (rowIndex >= rows.size()) {
        setError(QString("CSV row index out of range: %1").arg(rowIndex));
        return {};
    }

    m_lastError.clear();
    return rows.at(rowIndex);
}

int IOCSV::recordCount() const
{
    if (m_filePath.trimmed().isEmpty()) {
        setError(QStringLiteral("CSV file path is empty"));
        return 0;
    }

    QFile file(m_filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        setError(QString("failed to read CSV file '%1': %2")
                     .arg(m_filePath, file.errorString()));
        return 0;
    }

    QTextStream stream(&file);
    stream.setCodec("UTF-8");
    const int count = decodeRows(stream.readAll()).size();
    m_lastError.clear();
    return count;
}

bool IOCSV::isOpen() const
{
    return m_file && m_file->isOpen();
}

QString IOCSV::lastError() const
{
    return m_lastError;
}

QString IOCSV::encodeRow(const QStringList &fields)
{
    QStringList encodedFields;
    encodedFields.reserve(fields.size());

    for (const QString &field : fields) {
        QString encoded = field;
        encoded.replace(QStringLiteral("\""), QStringLiteral("\"\""));

        if (field.contains(QChar(','))
            || field.contains(QChar('"'))
            || field.contains(QChar('\n'))
            || field.contains(QChar('\r'))) {
            encoded = QStringLiteral("\"") + encoded + QStringLiteral("\"");
        }

        encodedFields.append(encoded);
    }

    return encodedFields.join(QChar(','));
}

QVector<QStringList> IOCSV::decodeRows(const QString &content)
{
    QVector<QStringList> rows;
    QStringList currentRow;
    QString currentField;
    bool insideQuotes = false;

    for (int index = 0; index < content.size(); ++index) {
        const QChar character = content.at(index);

        if (character == QChar('"')) {
            if (insideQuotes
                && index + 1 < content.size()
                && content.at(index + 1) == QChar('"')) {
                currentField.append(QChar('"'));
                ++index;
            } else {
                insideQuotes = !insideQuotes;
            }
            continue;
        }

        if (!insideQuotes && character == QChar(',')) {
            currentRow.append(currentField);
            currentField.clear();
            continue;
        }

        if (!insideQuotes && (character == QChar('\n') || character == QChar('\r'))) {
            if (character == QChar('\r')
                && index + 1 < content.size()
                && content.at(index + 1) == QChar('\n')) {
                ++index;
            }

            currentRow.append(currentField);
            currentField.clear();

            if (!currentRow.isEmpty()) {
                rows.append(currentRow);
            }
            currentRow.clear();
            continue;
        }

        currentField.append(character);
    }

    if (!currentField.isEmpty() || !currentRow.isEmpty()) {
        currentRow.append(currentField);
        rows.append(currentRow);
    }

    return rows;
}

void IOCSV::setError(const QString &errorMessage) const
{
    m_lastError = errorMessage;
}
