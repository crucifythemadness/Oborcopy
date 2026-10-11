#include "JobValidator.h"

#include <QDir>
#include <QFileInfo>

namespace obor::JobValidator {

namespace {

QString normalized(const QString &path)
{
    const QFileInfo info(path);
    QString result = info.canonicalFilePath();
    if (result.isEmpty())
        result = QDir::cleanPath(info.absoluteFilePath());
    if (!result.endsWith(QLatin1Char('/')))
        result += QLatin1Char('/');
    return result;
}

} // namespace

bool isSameOrInside(const QString &inner, const QString &outer)
{
#ifdef Q_OS_WIN
    const auto cs = Qt::CaseInsensitive;
#else
    const auto cs = Qt::CaseSensitive;
#endif
    return normalized(inner).startsWith(normalized(outer), cs);
}

QString validate(const JobOptions &job)
{
    if (job.source.trimmed().isEmpty())
        return QStringLiteral("Please choose a source folder.");
    if (job.destination.trimmed().isEmpty())
        return QStringLiteral("Please choose a destination folder.");
    if (!QFileInfo(job.source).isDir())
        return QStringLiteral("The source folder does not exist: %1").arg(job.source);
    if (QFileInfo::exists(job.destination) && !QFileInfo(job.destination).isDir())
        return QStringLiteral("The destination is not a folder: %1").arg(job.destination);
    if (isSameOrInside(job.destination, job.source))
        return QStringLiteral("The destination must not be the source folder or lie inside it.");
    if (isSameOrInside(job.source, job.destination))
        return QStringLiteral("The source must not lie inside the destination folder.");
    return {};
}

} // namespace obor::JobValidator
