#include "ServiceLog.h"

#include "AppPaths.h"

#include <QDateTime>
#include <QFile>
#include <QFileInfo>

#include <cstdio>

namespace obor {

namespace {

constexpr qint64 kMaxSize = 1024 * 1024;
constexpr int kKeep = 3;

QString rotatedName(const QString &path, int index)
{
    QString base = path;
    base.chop(4); // ".log"
    return QStringLiteral("%1.%2.log").arg(base).arg(index);
}

} // namespace

ServiceLog::ServiceLog(QString path)
    : m_path(std::move(path))
{
    AppPaths::ensureDir(QFileInfo(m_path).absolutePath());
}

void ServiceLog::info(const QString &text)
{
    write(QStringLiteral("INFO "), text);
}

void ServiceLog::error(const QString &text)
{
    write(QStringLiteral("ERROR"), text);
}

void ServiceLog::write(const QString &tag, const QString &text)
{
    const QMutexLocker lock(&m_mutex);
    rotateIfNeeded();
    const QString line = QStringLiteral("%1  %2  %3\n")
                             .arg(QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd HH:mm:ss")),
                                  tag, text);
    QFile file(m_path);
    if (file.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text))
        file.write(line.toUtf8());
    // Also visible in `journalctl --user -u oborcopy` and when run with --foreground.
    std::fputs(line.toLocal8Bit().constData(), stderr);
    std::fflush(stderr);
}

void ServiceLog::rotateIfNeeded()
{
    if (QFileInfo(m_path).size() < kMaxSize)
        return;
    QFile::remove(rotatedName(m_path, kKeep));
    for (int i = kKeep - 1; i >= 1; --i)
        QFile::rename(rotatedName(m_path, i), rotatedName(m_path, i + 1));
    QFile::rename(m_path, rotatedName(m_path, 1));
}

} // namespace obor
