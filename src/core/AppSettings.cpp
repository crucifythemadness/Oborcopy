#include "AppSettings.h"

#include "AppPaths.h"

#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>

namespace obor {

AppSettings AppSettings::load(const QString &path)
{
    AppSettings settings;
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return settings;
    const QJsonObject o = QJsonDocument::fromJson(file.readAll()).object();
    settings.theme = o.value("theme").toString(settings.theme);
    settings.lastJob = o.value("lastJob").toString();
    settings.windowGeometry = QByteArray::fromBase64(o.value("windowGeometry").toString().toLatin1());
    settings.splitterState = QByteArray::fromBase64(o.value("splitterState").toString().toLatin1());
    return settings;
}

bool AppSettings::save(const QString &path) const
{
    AppPaths::ensureDir(QFileInfo(path).absolutePath());
    const QJsonObject o{{"theme", theme},
                        {"lastJob", lastJob},
                        {"windowGeometry", QString::fromLatin1(windowGeometry.toBase64())},
                        {"splitterState", QString::fromLatin1(splitterState.toBase64())}};
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly))
        return false;
    file.write(QJsonDocument(o).toJson(QJsonDocument::Indented));
    return file.commit();
}

} // namespace obor
