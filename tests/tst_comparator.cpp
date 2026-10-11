#include "FileComparator.h"
#include "FsInfo.h"
#include "TestUtil.h"

#include <QTest>

using namespace obor;
using testutil::entry;

class TestComparator : public QObject
{
    Q_OBJECT

private slots:
    void exactMatch()
    {
        const FileComparator cmp(0, false);
        const qint64 t = testutil::baseTimeMs();
        QVERIFY(cmp.compare(entry("a", 10, t), entry("a", 10, t)).identical());
        QCOMPARE(cmp.compare(entry("a", 10, t + 1), entry("a", 10, t)).timeOrder, 1);
        QCOMPARE(cmp.compare(entry("a", 10, t - 1), entry("a", 10, t)).timeOrder, -1);
    }

    void fatTwoSecondTolerance()
    {
        const FileComparator cmp(2000, false);
        const qint64 t = testutil::baseTimeMs();
        QVERIFY(cmp.compare(entry("a", 10, t + 1999), entry("a", 10, t)).identical());
        QVERIFY(cmp.compare(entry("a", 10, t - 2000), entry("a", 10, t)).identical());
        QCOMPARE(cmp.compare(entry("a", 10, t + 2001), entry("a", 10, t)).timeOrder, 1);
    }

    void sizeChangeDetected()
    {
        const FileComparator cmp(2000, false);
        const qint64 t = testutil::baseTimeMs();
        const Comparison c = cmp.compare(entry("a", 11, t), entry("a", 10, t));
        QVERIFY(!c.sameSize);
        QCOMPARE(c.timeOrder, 0);
        QVERIFY(!c.identical());
    }

    void dstHourOffset()
    {
        const qint64 hour = 3600 * 1000;
        const qint64 t = testutil::baseTimeMs();
        QVERIFY(!FileComparator(2000, false).timesEqual(t + hour, t));
        QVERIFY(FileComparator(2000, true).timesEqual(t + hour, t));
        QVERIFY(FileComparator(2000, true).timesEqual(t - hour + 1000, t));
        QVERIFY(!FileComparator(2000, true).timesEqual(t + hour + 5000, t));
    }

    void autoTolerance()
    {
        VolumeInfo ntfs;
        ntfs.fileSystem = "NTFS";
        VolumeInfo exfat;
        exfat.fileSystem = "exFAT";
        exfat.fatLike = FsInfo::isFatLike(exfat.fileSystem);
        QVERIFY(exfat.fatLike);
        QVERIFY(FsInfo::isFatLike("vfat"));
        QVERIFY(!FsInfo::isFatLike("ext4"));
        QCOMPARE(FsInfo::autoToleranceMs(ntfs, ntfs), 0);
        QCOMPARE(FsInfo::autoToleranceMs(ntfs, exfat), 2000);
    }
};

QTEST_GUILESS_MAIN(TestComparator)
#include "tst_comparator.moc"
