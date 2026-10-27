#include "VolumeCheck.h"

#include <QTest>

using namespace obor;
using Status = VolumeCheck::Status;

namespace {

VolumeInfo volume(const QString &label, const QString &root)
{
    VolumeInfo info;
    info.valid = true;
    info.label = label;
    info.rootPath = root;
    return info;
}

const VolumeRef kBackup{"BACKUP", "E:/"};

} // namespace

class TestVolumeCheck : public QObject
{
    Q_OBJECT

private slots:
    void noIdentityWithoutRef()
    {
        const auto r = VolumeCheck::resolve("E:/Backup", VolumeRef(), volume("OTHER", "E:/"), {});
        QVERIFY(r.status == Status::NoIdentity);
        QCOMPARE(r.path, QString("E:/Backup"));
    }

    void noIdentityOutsideRoot()
    {
        const auto r = VolumeCheck::resolve("D:/Music", kBackup, volume("DATA", "D:/"), {});
        QVERIFY(r.status == Status::NoIdentity);
    }

    void okOnRememberedDisk()
    {
        const VolumeInfo e = volume("BACKUP", "E:/");
        const auto r = VolumeCheck::resolve("E:/Backup/Music", kBackup, e, {e});
        QVERIFY(r.status == Status::Ok);
        QCOMPARE(r.path, QString("E:/Backup/Music"));
    }

    void followsNewDriveLetter()
    {
        const auto r = VolumeCheck::resolve("E:/Backup/Music", kBackup, VolumeInfo(),
                                            {volume("BACKUP", "F:/")});
        QVERIFY(r.status == Status::Moved);
        QCOMPARE(r.path, QString("F:/Backup/Music"));
        QCOMPARE(r.volume.rootPath, QString("F:/"));
    }

    void followsWhenAnotherDiskTookTheLetter()
    {
        const auto r = VolumeCheck::resolve("E:/Backup", kBackup, volume("OTHER", "E:/"),
                                            {volume("BACKUP", "G:/")});
        QVERIFY(r.status == Status::Moved);
        QCOMPARE(r.path, QString("G:/Backup"));
    }

    void followsDriveRootItself()
    {
        const auto r = VolumeCheck::resolve("E:/", kBackup, VolumeInfo(), {volume("BACKUP", "F:/")});
        QVERIFY(r.status == Status::Moved);
        QCOMPARE(r.path, QString("F:/"));
    }

    void followsMountPoint()
    {
        const VolumeRef ref{"BACKUP", "/media/u/BACKUP"};
        const auto r = VolumeCheck::resolve("/media/u/BACKUP/Music", ref, volume("", "/"),
                                            {volume("BACKUP", "/media/u/BACKUP1")});
        QVERIFY(r.status == Status::Moved);
        QCOMPARE(r.path, QString("/media/u/BACKUP1/Music"));
    }

#ifdef Q_OS_WIN
    void driveLetterCaseIgnored()
    {
        const auto r = VolumeCheck::resolve("e:/Backup", kBackup, VolumeInfo(), {volume("BACKUP", "F:/")});
        QVERIFY(r.status == Status::Moved);
        QCOMPARE(r.path, QString("F:/Backup"));
    }
#endif

    void ambiguousLabel()
    {
        const auto r = VolumeCheck::resolve("E:/Backup", kBackup, VolumeInfo(),
                                            {volume("BACKUP", "F:/"), volume("BACKUP", "G:/")});
        QVERIFY(r.status == Status::Ambiguous);
    }

    void wrongDiskAtOldLetter()
    {
        const auto r = VolumeCheck::resolve("E:/Backup", kBackup, volume("OTHER", "E:/"), {});
        QVERIFY(r.status == Status::WrongDisk);
        QCOMPARE(r.volume.label, QString("OTHER"));
        QCOMPARE(r.path, QString("E:/Backup"));
    }

    void missingDisk()
    {
        const auto r = VolumeCheck::resolve("E:/Backup", kBackup, VolumeInfo(), {});
        QVERIFY(r.status == Status::Missing);
    }

    // An unmounted mount point resolves to the root file system: missing, not a wrong disk.
    void unmountedMountPointIsMissing()
    {
        const VolumeRef ref{"BACKUP", "/media/u/BACKUP"};
        const auto r = VolumeCheck::resolve("/media/u/BACKUP/Music", ref, volume("root", "/"), {});
        QVERIFY(r.status == Status::Missing);
    }

    void learnsFirstLabel()
    {
        const VolumeRef ref = VolumeCheck::learn(VolumeRef(), volume("BACKUP", "E:/"));
        QVERIFY(ref == kBackup);
    }

    void learnsNewRootOfSameLabel()
    {
        const VolumeRef ref = VolumeCheck::learn(kBackup, volume("BACKUP", "F:/"));
        QCOMPARE(ref.root, QString("F:/"));
    }

    void neverLearnsAnotherLabel()
    {
        QVERIFY(VolumeCheck::learn(kBackup, volume("OTHER", "E:/")) == kBackup);
    }

    void findsFolderOnOtherDrive()
    {
        const QList<VolumeInfo> volumes = {volume("WIN11", "C:/"), volume("AIMPII", "O:/"),
                                           volume("OTHER", "P:/")};
        const auto isDir = [](const QString &dir) { return dir == "O:/AIMP/FLAC"; };
        VolumeInfo found;
        QCOMPARE(VolumeCheck::findElsewhere("H:/AIMP/FLAC", volumes, isDir, &found),
                 QString("O:/AIMP/FLAC"));
        QCOMPARE(found.label, QString("AIMPII"));
        QCOMPARE(VolumeCheck::findElsewhere("H:\\AIMP\\FLAC", volumes, isDir, nullptr),
                 QString("O:/AIMP/FLAC"));
    }

    void noSuggestionWhenNotUnique()
    {
        const QList<VolumeInfo> volumes = {volume("A", "O:/"), volume("B", "P:/")};
        const auto everywhere = [](const QString &) { return true; };
        const auto nowhere = [](const QString &) { return false; };
        VolumeInfo found;
        QVERIFY(VolumeCheck::findElsewhere("H:/AIMP", volumes, everywhere, &found).isEmpty());
        QVERIFY(!found.valid);
        QVERIFY(VolumeCheck::findElsewhere("H:/AIMP", volumes, nowhere, &found).isEmpty());
    }

    void noSuggestionForOwnDriveRootsOrOtherPaths()
    {
        const auto everywhere = [](const QString &) { return true; };
        // Its own drive does not count (case-insensitive letter).
        QVERIFY(VolumeCheck::findElsewhere("h:/AIMP", {volume("X", "H:/")}, everywhere, nullptr).isEmpty());
        const QList<VolumeInfo> volumes = {volume("A", "O:/"), volume("M", "/media/u/M")};
        QVERIFY(VolumeCheck::findElsewhere("H:/", volumes, everywhere, nullptr).isEmpty());
        QVERIFY(VolumeCheck::findElsewhere("//server/share/x", volumes, everywhere, nullptr).isEmpty());
        QVERIFY(VolumeCheck::findElsewhere("/media/u/OLD/x", volumes, everywhere, nullptr).isEmpty());
    }

    void ignoresUnlabelledVolume()
    {
        QVERIFY(VolumeCheck::learn(VolumeRef(), volume("", "E:/")).isEmpty());
        QVERIFY(VolumeCheck::learn(kBackup, VolumeInfo()) == kBackup);
    }
};

QTEST_GUILESS_MAIN(TestVolumeCheck)
#include "tst_volumecheck.moc"
