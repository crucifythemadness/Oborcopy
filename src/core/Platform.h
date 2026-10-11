#pragma once

#include <QString>

// Thin OS abstraction for the few things Qt does not cover.
namespace obor::Platform {

// True when the file carries the Windows SYSTEM attribute (always false on Linux).
bool isSystemFile(const QString &path);

// Removes read-only/hidden/system attributes so the file can be replaced or deleted.
void clearProtectiveAttributes(const QString &path);

// Copies Windows attributes (read-only, hidden, system, archive). No-op on Linux.
bool copyAttributes(const QString &from, const QString &to);

// Atomically renames 'from' onto 'to', replacing an existing file.
bool replaceFile(const QString &from, const QString &to, QString *error);

// Sets the modification time of a directory (QFile cannot open directories).
bool setDirModificationTime(const QString &path, qint64 msecsSinceEpoch);

// Flushes the file's data to the storage device (fsync / FlushFileBuffers).
bool flushFileToDisk(const QString &path);

} // namespace obor::Platform
