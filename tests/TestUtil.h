#pragma once

#include "FileEntry.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTimeZone>

namespace testutil {

// Fixed reference time so tests do not depend on the clock.
inline qint64 baseTimeMs()
{
    return QDateTime(QDate(2025, 6, 1), QTime(12, 0, 0), QTimeZone::utc()).toMSecsSinceEpoch();
}

inline bool writeFile(const QString &path, const QByteArray &content, qint64 mtimeMs = baseTimeMs())
{
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return false;
    file.write(content);
    file.flush();
    file.setFileTime(QDateTime::fromMSecsSinceEpoch(mtimeMs, QTimeZone::utc()),
                     QFileDevice::FileModificationTime);
    return true;
}

inline QByteArray readFile(const QString &path)
{
    QFile file(path);
    return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray();
}

inline obor::FileEntry entry(const QString &rel, qint64 size, qint64 mtimeMs, bool dir = false)
{
    obor::FileEntry e;
    e.relPath = rel;
    e.size = size;
    e.mtimeMs = mtimeMs;
    e.isDir = dir;
    return e;
}

} // namespace testutil
