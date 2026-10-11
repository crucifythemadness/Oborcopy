#pragma once

#include "JobOptions.h"

#include <QDateTime>
#include <QList>
#include <QRegularExpression>

class QFileInfo;

namespace obor {

// Decides which files and folders take part in a run.
// Patterns without '/' match the name, patterns with '/' match the relative path.
class PathFilter
{
public:
    PathFilter(const FilterOptions &options, bool caseInsensitive,
               QDateTime now = QDateTime::currentDateTimeUtc());

    bool acceptDir(const QFileInfo &info, const QString &relPath) const;
    bool acceptFile(const QFileInfo &info, const QString &relPath) const;

    // Pure decision logic (no filesystem access), used by acceptFile and the tests.
    bool acceptDirName(const QString &name, const QString &relPath) const;
    bool acceptFileData(const QString &name, const QString &relPath, qint64 size,
                        qint64 mtimeMs) const;

private:
    struct Pattern
    {
        QRegularExpression regex;
        bool matchPath = false;
    };

    static QList<Pattern> compile(const QStringList &patterns, bool caseInsensitive);
    static bool matchesAny(const QList<Pattern> &patterns, const QString &name,
                           const QString &relPath);
    bool acceptAttributes(const QFileInfo &info) const;

    FilterOptions m_options;
    QList<Pattern> m_include;
    QList<Pattern> m_excludeFiles;
    QList<Pattern> m_excludeDirs;
    qint64 m_nowMs = 0;
};

} // namespace obor
