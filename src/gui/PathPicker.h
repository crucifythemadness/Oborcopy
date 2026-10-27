#pragma once

#include "FsInfo.h"
#include "VolumeCheck.h"

#include <QWidget>

class QLabel;
class QLineEdit;
class QTimer;

namespace obor {

// Folder input with browse button, drag & drop, an online/offline dot next to
// the caption and a drive info line ("BACKUP (E:) · exFAT · 412 GB free of 1.8 TB").
// Reachability is checked in the background (an offline network share can
// block for many seconds) after every change and every few seconds, so
// plugging or unplugging a drive shows up while the window is open.
// The disk the folder lives on is remembered by its label (VolumeCheck): a disk
// that comes back under another drive letter is followed, a different disk at
// the old letter is refused, and a disconnected one reads "Offline – connect BACKUP".
class PathPicker : public QWidget
{
    Q_OBJECT

public:
    enum class Availability {
        Empty,         // no path entered
        Checking,      // first check for this path still running
        Online,        // the folder exists
        WillBeCreated, // destination only: missing, but its parent folder exists
        Offline,       // folder (and for a destination its parent) not reachable
        WrongDisk      // another disk took the place of the remembered one
    };

    PathPicker(const QString &caption, QWidget *parent = nullptr);

    QString path() const;
    void setPath(const QString &path);
    void setPlaceholder(const QString &text);
    // Destination: a missing folder is fine as long as its parent exists.
    void setCreatable(bool creatable);

    Availability availability() const { return m_availability; }
    // Preview/Start may run with this folder.
    bool isUsable() const;

    // Disk the folder was last seen on; set before setPath().
    VolumeRef knownVolume() const { return m_knownVolume; }
    void setKnownVolume(const VolumeRef &ref);

signals:
    void pathChanged(const QString &path);
    void knownVolumeChanged();
    // The remembered disk was found under another drive letter or mount point.
    void volumeMoved(const QString &label, const QString &newRoot);
    void availabilityChanged(obor::PathPicker::Availability availability);

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    struct CheckResult
    {
        Availability availability = Availability::Offline;
        VolumeInfo volume;
        bool notAFolder = false; // the path exists but is a file
        VolumeCheck::Result disk;
        QString suggestion; // Offline, disk unknown: the folder under another drive letter
        VolumeInfo suggestionVolume;
    };

    void browse();
    void startCheck();
    void applyResult(quint64 generation, const QString &path, const CheckResult &result);
    void setAvailability(Availability availability, const VolumeInfo &volume);
    void useOtherDisk();
    static CheckResult check(const QString &path, bool creatable, const VolumeRef &ref);

    QString m_caption;
    QLineEdit *m_edit;
    QLabel *m_dot;
    QLabel *m_info;
    QTimer *m_infoTimer;
    QTimer *m_recheckTimer;
    bool m_creatable = false;
    bool m_checking = false;
    quint64 m_generation = 0;
    QString m_checkedPath; // path of the last finished check
    bool m_notAFolder = false;
    VolumeRef m_knownVolume;
    VolumeCheck::Status m_diskStatus = VolumeCheck::Status::NoIdentity;
    VolumeInfo m_otherDisk; // WrongDisk: the disk found instead
    QString m_suggestion;   // see CheckResult
    VolumeInfo m_suggestionVolume;
    bool m_following = false; // the path is being rewritten to a moved disk
    Availability m_availability = Availability::Empty;
};

} // namespace obor
