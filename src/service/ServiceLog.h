#pragma once

#include <QMutex>
#include <QString>

namespace obor {

// The scheduler's own log (service.log): one line per start, stop, schedule
// change and job run. Rotated at 1 MB, three old files are kept.
class ServiceLog
{
public:
    explicit ServiceLog(QString path);

    void info(const QString &text);
    void error(const QString &text);

private:
    void write(const QString &tag, const QString &text);
    void rotateIfNeeded();

    QString m_path;
    QMutex m_mutex;
};

} // namespace obor
