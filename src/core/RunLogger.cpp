#include "RunLogger.h"

#include "AppPaths.h"
#include "RunReport.h"

#include <QDateTime>
#include <QDir>
#include <QJsonDocument>
#include <QSaveFile>

namespace obor {

RunLogger::RunLogger(const LogOptions &options, const QString &logDir, const QString &jobName)
    : m_options(options)
    , m_dir(logDir)
    , m_jobName(jobName)
{
}

RunLogger::~RunLogger()
{
    close();
}

QString RunLogger::filePrefix(const QString &jobName)
{
    return QStringLiteral("Oborcopy_%1_").arg(AppPaths::sanitizeFileName(jobName));
}

bool RunLogger::open(QString *error)
{
    if (!AppPaths::ensureDir(m_dir)) {
        if (error)
            *error = QStringLiteral("Cannot create log folder %1").arg(m_dir);
        return false;
    }
    rotate(m_dir, m_jobName, qMax(1, m_options.keepLogs) - 1);
    const QString stamp = QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd-HHmmss"));
    QString base = m_dir + QLatin1Char('/') + filePrefix(m_jobName) + stamp;
    QString path = base + QStringLiteral(".log");
    for (int i = 2; QFile::exists(path); ++i)
        path = QStringLiteral("%1-%2.log").arg(base).arg(i);
    m_file.setFileName(path);
    if (!m_file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        if (error)
            *error = QStringLiteral("Cannot write log %1: %2").arg(path, m_file.errorString());
        return false;
    }
    return true;
}

void RunLogger::info(const QString &text)
{
    write(QStringLiteral("INFO "), text);
}

void RunLogger::error(const QString &text)
{
    write(QStringLiteral("ERROR"), text);
}

void RunLogger::change(const QString &text)
{
    if (m_options.level != LogLevel::Summary)
        write(QStringLiteral("     "), text);
}

void RunLogger::detail(const QString &text)
{
    if (m_options.level == LogLevel::AllFiles)
        write(QStringLiteral("     "), text);
}

void RunLogger::write(const QString &tag, const QString &text)
{
    const QString line = QStringLiteral("%1 %2 %3")
                             .arg(QDateTime::currentDateTime().toString(QStringLiteral("HH:mm:ss.zzz")),
                                  tag, text);
    QMutexLocker lock(&m_mutex);
    if (m_file.isOpen()) {
        m_file.write(line.toUtf8());
        m_file.write("\n");
    }
    if (m_echo)
        m_echo(line);
}

QString RunLogger::reportPath() const
{
    QString path = m_file.fileName();
    path.chop(4); // ".log"
    return path + QStringLiteral(".json");
}

bool RunLogger::writeReport(const RunReport &report)
{
    if (m_file.fileName().isEmpty())
        return false;
    QSaveFile file(reportPath());
    if (!file.open(QIODevice::WriteOnly))
        return false;
    file.write(QJsonDocument(report.toJson()).toJson(QJsonDocument::Compact));
    return file.commit();
}

void RunLogger::close()
{
    QMutexLocker lock(&m_mutex);
    if (m_file.isOpen())
        m_file.close();
}

void RunLogger::rotate(const QString &logDir, const QString &jobName, int keep)
{
    QDir dir(logDir);
    const QString prefix = filePrefix(jobName);
    const QStringList logs = dir.entryList({prefix + QStringLiteral("*.log")}, QDir::Files,
                                           QDir::Name | QDir::Reversed);
    for (int i = qMax(0, keep); i < logs.size(); ++i) {
        dir.remove(logs.at(i));
        QString json = logs.at(i);
        json.chop(4);
        dir.remove(json + QStringLiteral(".json"));
    }
}

} // namespace obor
