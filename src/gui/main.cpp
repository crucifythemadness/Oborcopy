#include "AppInfo.h"
#include "AppPaths.h"
#include "AppSettings.h"
#include "IconFactory.h"
#include "Language.h"
#include "MainWindow.h"
#include "Theme.h"

#include <QApplication>
#include <QProcess>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QApplication::setApplicationName(obor::AppInfo::name());
    QApplication::setApplicationVersion(obor::AppInfo::version());
    QApplication::setDesktopFileName(QStringLiteral("oborcopy"));
    QApplication::setWindowIcon(obor::IconFactory::appIcon());

    const obor::AppSettings settings = obor::AppSettings::load(obor::AppPaths::settingsFile());
    obor::Language::install(obor::Language::fromString(settings.language));
    obor::Theme::apply(obor::Theme::fromString(settings.theme));

    int result = 0;
    bool restart = false;
    {
        obor::MainWindow window;
        window.show();
        result = app.exec();
        restart = window.restartRequested();
    }
    // Language change: start a fresh instance, which builds every window in the new language.
    if (restart)
        QProcess::startDetached(QCoreApplication::applicationFilePath(), app.arguments().mid(1));
    return result;
}
