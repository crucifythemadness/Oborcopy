#include "AppInfo.h"
#include "AppPaths.h"
#include "AppSettings.h"
#include "IconFactory.h"
#include "MainWindow.h"
#include "Theme.h"

#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QApplication::setApplicationName(obor::AppInfo::name());
    QApplication::setApplicationVersion(obor::AppInfo::version());
    QApplication::setDesktopFileName(QStringLiteral("oborcopy"));
    QApplication::setWindowIcon(obor::IconFactory::appIcon());

    const obor::AppSettings settings = obor::AppSettings::load(obor::AppPaths::settingsFile());
    obor::Theme::apply(obor::Theme::fromString(settings.theme));

    obor::MainWindow window;
    window.show();
    return app.exec();
}
