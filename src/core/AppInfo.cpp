#include "AppInfo.h"

#include "version.h"

#include <QSysInfo>

const char *oborcopyBuildDate(); // generated: buildinfo.cpp

namespace obor::AppInfo {

QString name()
{
    return QStringLiteral("Oborcopy");
}

QString version()
{
    return QStringLiteral(OBORCOPY_VERSION);
}

QString buildDate()
{
    return QString::fromLatin1(oborcopyBuildDate());
}

QString qtVersion()
{
    return QString::fromLatin1(qVersion());
}

QString compiler()
{
#if defined(__clang__)
    return QStringLiteral("Clang %1.%2.%3").arg(__clang_major__).arg(__clang_minor__).arg(__clang_patchlevel__);
#elif defined(__GNUC__)
    return QStringLiteral("GCC %1.%2.%3").arg(__GNUC__).arg(__GNUC_MINOR__).arg(__GNUC_PATCHLEVEL__);
#elif defined(_MSC_VER)
    return QStringLiteral("MSVC %1").arg(_MSC_VER);
#else
    return QStringLiteral("unknown compiler");
#endif
}

QString platform()
{
    return QStringLiteral("%1 (%2)").arg(QSysInfo::prettyProductName(),
                                         QSysInfo::currentCpuArchitecture());
}

} // namespace obor::AppInfo
