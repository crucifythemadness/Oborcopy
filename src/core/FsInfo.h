#pragma once

#include <QString>

namespace obor {

struct VolumeInfo
{
    bool valid = false;
    QString fileSystem;   // e.g. NTFS, exFAT, ext4
    QString displayName;  // volume label or mount point
    QString rootPath;
    qint64 bytesFree = 0;
    qint64 bytesTotal = 0;
    bool fatLike = false; // 2-second timestamp resolution
};

// Information about the volume that holds a path (the path itself may not exist yet).
namespace FsInfo {

VolumeInfo query(const QString &path);
bool isFatLike(const QString &fileSystem);
// 2000 ms when either side is FAT/exFAT, otherwise 0 (exact comparison).
int autoToleranceMs(const VolumeInfo &source, const VolumeInfo &destination);
// File names compare case-insensitively on Windows and on FAT/exFAT volumes.
bool caseInsensitive(const VolumeInfo &source, const VolumeInfo &destination);

} // namespace FsInfo
} // namespace obor
