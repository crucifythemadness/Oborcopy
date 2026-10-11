#pragma once

#include "FileEntry.h"

#include <QString>
#include <QVector>

namespace obor {

enum class ActionType { CreateDir, CopyNew, Update, DeleteFile, DeleteDir, Skip };

struct SyncAction
{
    ActionType type = ActionType::Skip;
    QString relPath;
    qint64 size = 0;
    qint64 mtimeMs = 0;
    QFileDevice::Permissions permissions;
    QString reason; // e.g. "source newer", "extra in destination"
};

// The complete list of work for one run, computed before anything is changed.
struct SyncPlan
{
    QVector<SyncAction> deletions;  // files first, then folders deepest-first
    QVector<SyncAction> createDirs; // shallowest first
    QVector<SyncAction> copies;     // CopyNew and Update
    QVector<SyncAction> skipped;    // only filled when the log level asks for it
    QVector<SyncAction> dirTimes;   // source folders whose timestamps are applied at the end
    QStringList staleTemps;         // leftovers of interrupted runs, removed silently
    QVector<ScanError> scanErrors;

    int filesChecked = 0;
    int unchanged = 0;
    int destFileCount = 0;
    qint64 bytesToCopy = 0;
    qint64 destBytesFree = -1;

    bool thresholdExceeded = false;
    double deletionPercent = 0.0;

    int count(ActionType type) const;
    bool hasWork() const;
    bool hasDeletions() const { return !deletions.isEmpty(); }
    bool spaceWarning() const { return destBytesFree >= 0 && bytesToCopy > destBytesFree; }
};

QString toString(ActionType type);

} // namespace obor
