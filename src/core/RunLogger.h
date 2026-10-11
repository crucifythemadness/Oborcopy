#pragma once

#include "JobOptions.h"

#include <QFile>
#include <QMutex>
#include <QString>

#include <functional>

namespace obor {

struct RunReport;

// Writes one log file per run: <logDir>/Oborcopy_<job>_<yyyyMMdd-HHmmss>.log
// plus the run report as .json with the same base name. Thread-safe.
class RunLogger
{
public:
    using EchoFn = std::function<void(const QString &line)>;

    RunLogger(const LogOptions &options, const QString &logDir, const QString &jobName);
    ~RunLogger();

    bool open(QString *error = nullptr);
    void setEcho(EchoFn echo) { m_echo = std::move(echo); }

    void info(const QString &text);    // always written
    void error(const QString &text);   // always written
    void change(const QString &text);  // level Changes and above
    void detail(const QString &text);  // level AllFiles only

    QString filePath() const { return m_file.fileName(); }
    QString reportPath() const;
    bool writeReport(const RunReport &report);
    void close();

    // Deletes the oldest logs of this job so that at most 'keep' remain.
    static void rotate(const QString &logDir, const QString &jobName, int keep);
    static QString filePrefix(const QString &jobName);

private:
    void write(const QString &tag, const QString &text);

    LogOptions m_options;
    QString m_dir;
    QString m_jobName;
    QFile m_file;
    QMutex m_mutex;
    EchoFn m_echo;
};

} // namespace obor
