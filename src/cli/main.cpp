#include "AppInfo.h"
#include "CliRunner.h"

#include <QCoreApplication>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

int main(int argc, char *argv[])
{
#ifdef Q_OS_WIN
    SetConsoleOutputCP(CP_UTF8); // file names with umlauts etc. print correctly
#endif
    QCoreApplication app(argc, argv);
    QCoreApplication::setApplicationName(obor::AppInfo::name());
    QCoreApplication::setApplicationVersion(obor::AppInfo::version());

    obor::CliRunner runner;
    return runner.run(QCoreApplication::arguments());
}
