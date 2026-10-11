#pragma once

#include <QByteArray>
#include <QString>

namespace obor {

class SyncControl;

namespace Hash {

// Streams the file through SHA-256. Returns an empty array on error or cancel.
QByteArray fileSha256(const QString &path, SyncControl *control, QString *error = nullptr);

} // namespace Hash
} // namespace obor
