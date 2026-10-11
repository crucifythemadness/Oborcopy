#pragma once

#include <QByteArray>
#include <QString>

namespace obor {

// Application-wide preferences (not job specific), stored in settings.json.
struct AppSettings
{
    QString theme = QStringLiteral("system"); // system | light | dark
    QString lastJob;
    QByteArray windowGeometry;
    QByteArray splitterState;

    static AppSettings load(const QString &path);
    bool save(const QString &path) const;
};

} // namespace obor
