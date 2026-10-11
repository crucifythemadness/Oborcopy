#include "JobOptions.h"

#include "AppPaths.h"

namespace obor {

JobOptions JobOptions::defaults(const QString &name)
{
    JobOptions job;
    job.name = name;
    job.filter.excludeDirs = {QStringLiteral("$RECYCLE.BIN"),
                              QStringLiteral("System Volume Information"),
                              QStringLiteral(".Trash-*")};
    return job;
}

QString JobOptions::effectiveLogDir() const
{
    return log.logDir.isEmpty() ? AppPaths::defaultLogDir() : log.logDir;
}

QString toString(SyncMode mode)
{
    return mode == SyncMode::Mirror ? QStringLiteral("Mirror") : QStringLiteral("Update");
}

} // namespace obor
