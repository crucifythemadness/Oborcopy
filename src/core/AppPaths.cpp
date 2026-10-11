#include "AppPaths.h"

#include <QDir>
#include <QRegularExpression>
#include <QStandardPaths>

namespace obor::AppPaths {

static QString overrideDir()
{
    return qEnvironmentVariable("OBORCOPY_CONFIG_DIR");
}

QString configDir()
{
    const QString custom = overrideDir();
    if (!custom.isEmpty())
        return QDir::cleanPath(custom);
    return QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
}

QString jobsDir()
{
    return configDir() + QStringLiteral("/jobs");
}

QString settingsFile()
{
    return configDir() + QStringLiteral("/settings.json");
}

QString defaultLogDir()
{
    const QString custom = overrideDir();
    if (!custom.isEmpty())
        return QDir::cleanPath(custom) + QStringLiteral("/logs");
    return QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation)
           + QStringLiteral("/logs");
}

bool ensureDir(const QString &path)
{
    return QDir().mkpath(path);
}

QString sanitizeFileName(const QString &name)
{
    static const QRegularExpression invalid(QStringLiteral(R"([<>:"/\\|?*\x00-\x1F])"));
    QString result = name.trimmed();
    result.replace(invalid, QStringLiteral("_"));
    while (result.endsWith(QLatin1Char('.')) || result.endsWith(QLatin1Char(' ')))
        result.chop(1);
    return result.isEmpty() ? QStringLiteral("job") : result;
}

} // namespace obor::AppPaths
