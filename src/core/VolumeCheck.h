#pragma once

#include "FsInfo.h"

#include <QList>
#include <QString>

#include <functional>

// Finds the disk a job folder belongs to by its label rather than its drive
// letter: follows a backup disk that came back as another letter or mount
// point, and refuses a different disk that took over the old letter.
namespace obor::VolumeCheck {

enum class Status {
    NoIdentity, // no remembered disk, or the path is not on it: not checked
    Ok,         // the remembered disk holds the path
    Moved,      // the disk is connected under another root; 'path' points there
    Ambiguous,  // several connected disks carry the label
    WrongDisk,  // another disk is mounted where the remembered one was
    Missing     // the disk is not connected
};

struct Result
{
    Status status = Status::NoIdentity;
    QString path;      // the path to use (rewritten for Moved)
    VolumeInfo volume; // Ok/Moved: the remembered disk; WrongDisk: the other disk
};

// Pure decision: 'atPath' is FsInfo::query(path), 'named' is FsInfo::volumesNamed(ref.label).
Result resolve(const QString &path, const VolumeRef &ref, const VolumeInfo &atPath,
               const QList<VolumeInfo> &named);
// Same, querying the mounted volumes.
Result resolve(const QString &path, const VolumeRef &ref);

// The reference to remember after 'volume' was seen holding the folder. A
// labelled volume is learnt when nothing is known yet or the label matches
// (the root may have changed); a different label never replaces the old one.
VolumeRef learn(const VolumeRef &ref, const VolumeInfo &volume);

// For an unreachable "X:/rest" whose disk is not known yet: the same folder on
// exactly one other drive letter ("O:/rest", its volume in 'found'), else empty.
// Only a suggestion – without a known label it may be a different copy.
QString findElsewhere(const QString &path, const QList<VolumeInfo> &volumes,
                      const std::function<bool(const QString &)> &isDir, VolumeInfo *found);
// Same, over the mounted volumes and the real file system.
QString findElsewhere(const QString &path, VolumeInfo *found);

} // namespace obor::VolumeCheck
