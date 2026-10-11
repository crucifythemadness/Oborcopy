#include "SyncPlanner.h"
#include "TestUtil.h"

#include <QTest>

using namespace obor;
using testutil::entry;

class TestPlanner : public QObject
{
    Q_OBJECT

private:
    static void addFile(ScanResult &scan, const QString &rel, qint64 size, qint64 mtime)
    {
        scan.files.insert(rel, entry(rel, size, mtime));
    }
    static void addDir(ScanResult &scan, const QString &rel)
    {
        scan.dirs.insert(rel, entry(rel, 0, testutil::baseTimeMs(), true));
    }
    static SyncPlan plan(const JobOptions &job, const ScanResult &src, const ScanResult &dst,
                         int toleranceMs = 2000)
    {
        const FileComparator cmp(toleranceMs, false);
        return SyncPlanner(job, cmp).build(src, dst);
    }

private slots:
    void newChangedAndUnchanged()
    {
        const qint64 t = testutil::baseTimeMs();
        ScanResult src, dst;
        addFile(src, "new.mp3", 10, t);
        addFile(src, "same.mp3", 10, t);
        addFile(src, "fat.mp3", 10, t + 1000); // within FAT tolerance
        addFile(src, "newer.mp3", 10, t + 60000);
        addFile(src, "resized.mp3", 12, t);
        addFile(dst, "same.mp3", 10, t);
        addFile(dst, "fat.mp3", 10, t);
        addFile(dst, "newer.mp3", 10, t);
        addFile(dst, "resized.mp3", 10, t);

        const SyncPlan p = plan(JobOptions::defaults("t"), src, dst);
        QCOMPARE(p.filesChecked, 5);
        QCOMPARE(p.count(ActionType::CopyNew), 1);
        QCOMPARE(p.count(ActionType::Update), 2);
        QCOMPARE(p.unchanged, 2);
        QCOMPARE(p.bytesToCopy, qint64(10 + 10 + 12));
        QVERIFY(p.deletions.isEmpty());
    }

    void updateModeNeverDeletes()
    {
        ScanResult src, dst;
        addFile(dst, "extra.mp3", 1, testutil::baseTimeMs());
        addFile(src, "keep.mp3", 1, testutil::baseTimeMs());
        const SyncPlan p = plan(JobOptions::defaults("t"), src, dst);
        QVERIFY(p.deletions.isEmpty());
    }

    void mirrorDeletesExtrasFilesFirstDeepestDirsFirst()
    {
        const qint64 t = testutil::baseTimeMs();
        ScanResult src, dst;
        addFile(src, "keep.mp3", 1, t);
        addFile(dst, "keep.mp3", 1, t);
        addDir(dst, "old");
        addDir(dst, "old/deeper");
        addFile(dst, "old/deeper/x.mp3", 1, t);
        JobOptions job = JobOptions::defaults("t");
        job.mode = SyncMode::Mirror;
        job.safety.thresholdEnabled = false;

        const SyncPlan p = plan(job, src, dst);
        QCOMPARE(p.deletions.size(), 3);
        QCOMPARE(p.deletions[0].relPath, QString("old/deeper/x.mp3"));
        QCOMPARE(p.deletions[1].relPath, QString("old/deeper"));
        QCOMPARE(p.deletions[2].relPath, QString("old"));
    }

    void deletionThreshold()
    {
        const qint64 t = testutil::baseTimeMs();
        ScanResult src, dst;
        for (int i = 0; i < 10; ++i)
            addFile(dst, QString("f%1").arg(i), 1, t);
        for (int i = 0; i < 7; ++i)
            addFile(src, QString("f%1").arg(i), 1, t);
        JobOptions job = JobOptions::defaults("t");
        job.mode = SyncMode::Mirror;
        job.safety.thresholdPercent = 20;
        const SyncPlan p = plan(job, src, dst);
        QCOMPARE(p.count(ActionType::DeleteFile), 3);
        QVERIFY(qAbs(p.deletionPercent - 30.0) < 0.01);
        QVERIFY(p.thresholdExceeded);

        job.safety.thresholdPercent = 50;
        QVERIFY(!plan(job, src, dst).thresholdExceeded);
    }

    void skipOlderAndNewer()
    {
        const qint64 t = testutil::baseTimeMs();
        ScanResult src, dst;
        addFile(src, "older.mp3", 5, t - 60000);
        addFile(src, "newer.mp3", 5, t + 60000);
        addFile(dst, "older.mp3", 5, t);
        addFile(dst, "newer.mp3", 5, t);
        JobOptions job = JobOptions::defaults("t");
        QCOMPARE(plan(job, src, dst).copies.size(), 2);
        job.copy.skipOlder = true;
        QCOMPARE(plan(job, src, dst).copies.size(), 1);
        job.copy.skipNewer = true;
        QCOMPARE(plan(job, src, dst).copies.size(), 0);
    }

    void checksumDetectsSilentChange()
    {
        const qint64 t = testutil::baseTimeMs();
        ScanResult src, dst;
        addFile(src, "a.mp3", 5, t);
        addFile(dst, "a.mp3", 5, t);
        JobOptions job = JobOptions::defaults("t");
        job.copy.compareChecksum = true;
        const FileComparator cmp(0, false);
        const SyncPlan p = SyncPlanner(job, cmp, [](const QString &) { return true; }).build(src, dst);
        QCOMPARE(p.count(ActionType::Update), 1);
        QCOMPARE(p.copies.first().reason, QString("content differs"));
    }

    void createDirsShallowFirst()
    {
        ScanResult src, dst;
        addDir(src, "a/b/c");
        addDir(src, "a");
        addDir(src, "a/b");
        const SyncPlan p = plan(JobOptions::defaults("t"), src, dst);
        QCOMPARE(p.createDirs.size(), 3);
        QCOMPARE(p.createDirs[0].relPath, QString("a"));
        QCOMPARE(p.createDirs[2].relPath, QString("a/b/c"));
    }
};

QTEST_GUILESS_MAIN(TestPlanner)
#include "tst_planner.moc"
