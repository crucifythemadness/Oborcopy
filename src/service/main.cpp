// oborcopy-service: the Oborcopy Scheduler. Installed and controlled from the
// Oborcopy application (Service -> Service Manager); runs scheduled jobs with
// oborcopy-cli. Usage:
//   oborcopy-service                 (Windows) started by the service control manager
//   oborcopy-service --foreground    run in the console / under systemd
//   oborcopy-service --install --user <DOMAIN\name> [--result-file <path>]   (Windows, elevated)
//   oborcopy-service --uninstall | --update-password --user <name> | --set-path   (Windows, elevated)

#include "AppInfo.h"
#include "ServiceFiles.h"

#include <QCoreApplication>
#include <QStringList>

#include <cstdio>

#ifdef Q_OS_WIN
#include "WinService.h"
#include <windows.h>
#else
#include "PosixDaemon.h"
#endif

namespace {

[[maybe_unused]] QString value(const QStringList &args, const QString &name)
{
    const int i = args.indexOf(name);
    return (i >= 0 && i + 1 < args.size()) ? args.at(i + 1) : QString();
}

int usage()
{
    const QString text =
        QStringLiteral("%1 %2 - %3\n\n"
                       "This program is installed and controlled from the Oborcopy application\n"
                       "(menu Service > Service Manager).\n\n"
                       "  --foreground   run the scheduler in this console (Ctrl+C stops it)\n"
                       "  --version      show the version\n")
            .arg(obor::ServiceInfo::displayName(), obor::AppInfo::version(), obor::ServiceInfo::description());
    std::fputs(text.toLocal8Bit().constData(), stdout);
    return 0;
}

} // namespace

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    // Same name as the GUI, so both resolve the same configuration folder.
    QCoreApplication::setApplicationName(obor::AppInfo::name());
    QCoreApplication::setApplicationVersion(obor::AppInfo::version());
    const QStringList args = QCoreApplication::arguments();

    if (args.contains(QStringLiteral("--version"))) {
        std::printf("%s\n", obor::AppInfo::version().toLocal8Bit().constData());
        return 0;
    }
    if (args.contains(QStringLiteral("--help")) || args.contains(QStringLiteral("-h")))
        return usage();

#ifdef Q_OS_WIN
    const QString resultFile = value(args, QStringLiteral("--result-file"));
    const QString user = value(args, QStringLiteral("--user"));
    if (args.contains(QStringLiteral("--install")))
        return obor::WinService::install(user, resultFile);
    if (args.contains(QStringLiteral("--uninstall")))
        return obor::WinService::uninstall(resultFile);
    if (args.contains(QStringLiteral("--update-password")))
        return obor::WinService::updatePassword(user, resultFile);
    if (args.contains(QStringLiteral("--set-path")))
        return obor::WinService::setPath(resultFile);
    if (args.contains(QStringLiteral("--foreground"))) {
        SetConsoleOutputCP(CP_UTF8);
        return obor::WinService::runConsole();
    }
    const int code = obor::WinService::runDispatcher();
    return code == -1 ? usage() : code;
#else
    if (args.contains(QStringLiteral("--foreground")))
        return obor::PosixDaemon::run();
    return usage();
#endif
}
