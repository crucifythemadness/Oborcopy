#include "PathFilter.h"

#include "Platform.h"

#include <QFileInfo>

namespace obor {

namespace {
constexpr qint64 kDayMs = 24LL * 60 * 60 * 1000;
}

PathFilter::PathFilter(const FilterOptions &options, bool caseInsensitive, QDateTime now)
    : m_options(options)
    , m_include(compile(options.includeFiles, caseInsensitive))
    , m_excludeFiles(compile(options.excludeFiles, caseInsensitive))
    , m_excludeDirs(compile(options.excludeDirs, caseInsensitive))
    , m_nowMs(now.toMSecsSinceEpoch())
{
}

QList<PathFilter::Pattern> PathFilter::compile(const QStringList &patterns, bool caseInsensitive)
{
    QList<Pattern> result;
    const auto cs = caseInsensitive ? Qt::CaseInsensitive : Qt::CaseSensitive;
    for (QString pattern : patterns) {
        pattern = pattern.trimmed();
        pattern.replace(QLatin1Char('\\'), QLatin1Char('/'));
        while (pattern.startsWith(QLatin1Char('/')))
            pattern.remove(0, 1);
        if (pattern.isEmpty())
            continue;
        Pattern p;
        p.matchPath = pattern.contains(QLatin1Char('/'));
        // Default conversion: '*' does not cross '/' — like a shell glob.
        p.regex = QRegularExpression::fromWildcard(pattern, cs);
        result << p;
    }
    return result;
}

bool PathFilter::matchesAny(const QList<Pattern> &patterns, const QString &name,
                            const QString &relPath)
{
    for (const Pattern &p : patterns) {
        if (p.regex.match(p.matchPath ? relPath : name).hasMatch())
            return true;
    }
    return false;
}

bool PathFilter::acceptAttributes(const QFileInfo &info) const
{
    if (m_options.skipHidden && info.isHidden())
        return false;
    if (m_options.skipSystem && Platform::isSystemFile(info.absoluteFilePath()))
        return false;
    return true;
}

bool PathFilter::acceptDir(const QFileInfo &info, const QString &relPath) const
{
    return acceptDirName(info.fileName(), relPath) && acceptAttributes(info);
}

bool PathFilter::acceptFile(const QFileInfo &info, const QString &relPath) const
{
    return acceptAttributes(info)
           && acceptFileData(info.fileName(), relPath, info.size(),
                             info.lastModified().toMSecsSinceEpoch());
}

bool PathFilter::acceptDirName(const QString &name, const QString &relPath) const
{
    return !matchesAny(m_excludeDirs, name, relPath);
}

bool PathFilter::acceptFileData(const QString &name, const QString &relPath, qint64 size,
                                qint64 mtimeMs) const
{
    if (!m_include.isEmpty() && !matchesAny(m_include, name, relPath))
        return false;
    if (matchesAny(m_excludeFiles, name, relPath))
        return false;
    if (m_options.minSize > 0 && size < m_options.minSize)
        return false;
    if (m_options.maxSize > 0 && size > m_options.maxSize)
        return false;
    const qint64 ageMs = m_nowMs - mtimeMs;
    if (m_options.minAgeDays > 0 && ageMs < m_options.minAgeDays * kDayMs)
        return false;
    if (m_options.maxAgeDays > 0 && ageMs > m_options.maxAgeDays * kDayMs)
        return false;
    return true;
}

} // namespace obor
