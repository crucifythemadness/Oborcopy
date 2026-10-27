#pragma once

#include <QList>
#include <QString>

namespace obor {

// The disk a job folder lives on, remembered when it was last reachable. Disks
// are recognised by their label, so a backup disk is still found after it got
// another drive letter or mount point.
struct VolumeRef
{
    QString label; // e.g. "BACKUP"
    QString root;  // drive or mount point when last seen, e.g. "E:/" or "/media/u/BACKUP"

    bool isEmpty() const { return label.isEmpty(); }
    bool operator==(const VolumeRef &other) const = default;
};

struct VolumeInfo
{
    bool valid = false;
    QString fileSystem;   // e.g. NTFS, exFAT, ext4
    QString label;        // volume label, may be empty
    QString rootPath;     // drive or mount point, e.g. "E:/" or "/media/usb"
    qint64 bytesFree = 0;
    qint64 bytesTotal = 0;
    bool fatLike = false; // 2-second timestamp resolution
};

// Information about the volume that holds a path (the path itself may not exist yet).
namespace FsInfo {

VolumeInfo query(const QString &path);
// All mounted, ready volumes.
QList<VolumeInfo> mountedVolumes();
// All mounted, ready volumes with this label.
QList<VolumeInfo> volumesNamed(const QString &label);
bool isFatLike(const QString &fileSystem);
// "BACKUP (E:)" on Windows, "BACKUP" elsewhere; the drive or mount point without a label.
QString volumeName(const VolumeInfo &volume);
// 2000 ms when either side is FAT/exFAT, otherwise 0 (exact comparison).
int autoToleranceMs(const VolumeInfo &source, const VolumeInfo &destination);
// File names compare case-insensitively on Windows and on FAT/exFAT volumes.
bool caseInsensitive(const VolumeInfo &source, const VolumeInfo &destination);

} // namespace FsInfo
} // namespace obor
