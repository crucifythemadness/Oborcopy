#pragma once

#include <QFileDevice>
#include <QHash>
#include <QString>
#include <QStringList>
#include <QVector>

namespace obor {

// Suffix of in-progress copies; such files are never treated as user data.
inline const QString kTempSuffix = QStringLiteral(".oborcopy-tmp");
// Folder (in the destination root) that receives deleted files in Archive mode.
inline const QString kArchiveDirName = QStringLiteral(".oborcopy-archive");

struct FileEntry
{
    QString relPath; // '/' separated, original spelling
    qint64 size = 0;
    qint64 mtimeMs = 0; // UTC milliseconds since epoch
    bool isDir = false;
    QFileDevice::Permissions permissions;
};

struct ScanError
{
    QString relPath;
    QString message;
};

struct ScanResult
{
    QHash<QString, FileEntry> files; // keyed by compareKey()
    QHash<QString, FileEntry> dirs;
    QStringList staleTemps;          // leftovers of interrupted copies
    QVector<ScanError> errors;
    bool rootExists = false;
    bool cancelled = false;
};

inline QString compareKey(const QString &relPath, bool caseInsensitive)
{
    return caseInsensitive ? relPath.toCaseFolded() : relPath;
}

} // namespace obor
