#include "RunLogger.h"
#include "SyncControl.h"
#include "SyncEngine.h"
#include "TestUtil.h"

#include <QDirIterator>
#include <QTemporaryDir>
#include <QTest>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

using namespace obor;
using testutil::readFile;
using testutil::writeFile;

// End-to-end tests: real files in a temporary folder, the full engine pipeline.
class TestSyncEngine : public QObject
{
    Q_OBJECT

private:
    QTemporaryDir m_root;
    QString m_src;
    QString m_dst;

    JobOptions job(SyncMode mode = SyncMode::Update) const
    {
        JobOptions j = JobOptions::defaults("test");
        j.source = m_src;
        j.destination = m_dst;
        j.mode = mode;
        j.retry = {1, 10, 20};
        j.safety.thresholdEnabled = false;
        return j;
    }

    static RunReport run(const JobOptions &j, SyncCallbacks callbacks = {})
    {
        SyncControl control;
        return SyncEngine(j, control, std::move(callbacks)).run();
    }

    QStringList tempFiles() const
    {
        QStringList found;
        QDirIterator it(m_dst, {"*.oborcopy-tmp"}, QDir::Files | QDir::Hidden,
                        QDirIterator::Subdirectories);
        while (it.hasNext())
            found << it.next();
        return found;
    }

    void createSampleSource()
    {
        QVERIFY(writeFile(m_src + "/Artist A/Album 1/01 Intro.mp3", "intro-data"));
        QVERIFY(writeFile(m_src + "/Artist A/Album 1/02 Song.mp3", "song-data"));
        QVERIFY(writeFile(m_src + "/Artist B/Track.flac", "flac-data"));
        QVERIFY(QDir().mkpath(m_src + "/Empty Folder"));
    }

private slots:
    void initTestCase()
    {
        QVERIFY(m_root.isValid());
        qputenv("OBORCOPY_CONFIG_DIR", (m_root.path() + "/config").toUtf8());
    }

    void init()
    {
        static int counter = 0;
        ++counter;
        m_src = QString("%1/case%2/src").arg(m_root.path()).arg(counter);
        m_dst = QString("%1/case%2/dst").arg(m_root.path()).arg(counter);
        QVERIFY(QDir().mkpath(m_src));
    }

    void fullCopyThenNothingToDo()
    {
        createSampleSource();
        RunReport r = run(job());
        QVERIFY(r.status == RunStatus::Completed);
        QCOMPARE(r.stats.filesChecked, 3);
        QCOMPARE(r.stats.copied, 3);
        QCOMPARE(readFile(m_dst + "/Artist B/Track.flac"), QByteArray("flac-data"));
        QVERIFY(QFileInfo(m_dst + "/Empty Folder").isDir());
        QCOMPARE(QFileInfo(m_dst + "/Artist B/Track.flac").lastModified().toMSecsSinceEpoch(),
                 testutil::baseTimeMs());

        r = run(job());
        QCOMPARE(r.stats.copied, 0);
        QCOMPARE(r.stats.updated, 0);
        QCOMPARE(r.stats.unchanged, 3);
    }

    void mirrorCopiesUpdatesAndDeletes()
    {
        createSampleSource();
        run(job(SyncMode::Mirror));
        QVERIFY(writeFile(m_src + "/Artist B/Track.flac", "flac-data-v2", testutil::baseTimeMs() + 60000));
        QVERIFY(writeFile(m_src + "/Artist C/New.mp3", "new"));
        QVERIFY(QFile::remove(m_src + "/Artist A/Album 1/01 Intro.mp3"));

        const RunReport r = run(job(SyncMode::Mirror));
        QCOMPARE(r.stats.copied, 1);
        QCOMPARE(r.stats.updated, 1);
        QCOMPARE(r.stats.deleted, 1);
        QCOMPARE(r.stats.errors, 0);
        QVERIFY(!QFile::exists(m_dst + "/Artist A/Album 1/01 Intro.mp3"));
        QCOMPARE(readFile(m_dst + "/Artist B/Track.flac"), QByteArray("flac-data-v2"));
        QCOMPARE(r.summaryLine(), QString("3 files checked, 1 copied, 1 updated, 1 deleted, 0 errors"));
    }

    void updateModeKeepsExtras()
    {
        createSampleSource();
        QVERIFY(writeFile(m_dst + "/only-in-backup.txt", "x"));
        const RunReport r = run(job(SyncMode::Update));
        QCOMPARE(r.stats.deleted, 0);
        QVERIFY(QFile::exists(m_dst + "/only-in-backup.txt"));
    }

    void dryRunChangesNothing()
    {
        createSampleSource();
        JobOptions j = job(SyncMode::Mirror);
        j.copy.dryRun = true;
        const RunReport r = run(j);
        QVERIFY(r.dryRun);
        QCOMPARE(r.stats.copied, 3);
        QVERIFY(!QFileInfo::exists(m_dst));
    }

    void confirmationRejectedChangesNothing()
    {
        createSampleSource();
        SyncCallbacks cb;
        bool asked = false;
        cb.confirm = [&asked](const SyncPlan &plan) {
            asked = plan.copies.size() == 3;
            return false;
        };
        const RunReport r = run(job(), cb);
        QVERIFY(asked);
        QVERIFY(r.status == RunStatus::Cancelled);
        QVERIFY(!QFileInfo::exists(m_dst + "/Artist B/Track.flac"));
    }

    void emptySourceIsRefusedInMirror()
    {
        QVERIFY(writeFile(m_dst + "/precious.mp3", "keep me"));
        const RunReport r = run(job(SyncMode::Mirror));
        QVERIFY(r.status == RunStatus::Failed);
        QVERIFY(QFile::exists(m_dst + "/precious.mp3"));
    }

    void destinationInsideSourceIsRejected()
    {
        JobOptions j = job();
        j.destination = m_src + "/backup";
        const RunReport r = run(j);
        QVERIFY(r.status == RunStatus::Failed);
        QVERIFY(!r.fatalError.isEmpty());
    }

    // The destination's drive now holds another disk than the remembered one.
    void differentDiskIsRefused()
    {
        createSampleSource();
        QVERIFY(writeFile(m_dst + "/precious.mp3", "keep me"));
        JobOptions j = job(SyncMode::Mirror);
        j.destinationVolume = {"NO-SUCH-DISK-7Q", FsInfo::query(m_dst).rootPath};
        const RunReport r = run(j);
        QVERIFY(r.status == RunStatus::Failed);
        QVERIFY(r.fatalError.contains("NO-SUCH-DISK-7Q"));
        QVERIFY(QFile::exists(m_dst + "/precious.mp3"));
    }

    void archiveModeKeepsDeletedFiles()
    {
        QVERIFY(writeFile(m_src + "/keep.mp3", "k"));
        QVERIFY(writeFile(m_dst + "/gone.mp3", "g"));
        JobOptions j = job(SyncMode::Mirror);
        j.safety.deleteMode = DeleteMode::Archive;
        const RunReport r = run(j);
        QCOMPARE(r.stats.deleted, 1);
        QVERIFY(!QFile::exists(m_dst + "/gone.mp3"));
        QDirIterator it(m_dst + "/.oborcopy-archive", {"gone.mp3"}, QDir::Files,
                        QDirIterator::Subdirectories);
        QVERIFY(it.hasNext());
        // A second run must not treat the archive as an extra folder.
        QCOMPARE(run(j).stats.deleted, 0);
    }

    void staleTempFilesAreRemoved()
    {
        QVERIFY(writeFile(m_src + "/a.mp3", "a"));
        QVERIFY(writeFile(m_dst + "/a.mp3.oborcopy-tmp", "half"));
        const RunReport r = run(job());
        QCOMPARE(r.stats.copied, 1);
        QVERIFY(tempFiles().isEmpty());
    }

    void parallelCopies()
    {
        for (int i = 0; i < 40; ++i)
            QVERIFY(writeFile(QString("%1/dir%2/file%3.bin").arg(m_src).arg(i % 5).arg(i),
                              QByteArray(1000 + i, char('a' + i % 26))));
        JobOptions j = job();
        j.copy.parallelCopies = 4;
        j.copy.verifyAfterCopy = true;
        const RunReport r = run(j);
        QCOMPARE(r.stats.copied, 40);
        QCOMPARE(r.stats.errors, 0);
        QCOMPARE(readFile(m_dst + "/dir3/file13.bin").size(), 1013);
    }

    void cancelDuringCopyLeavesNoTempFile()
    {
        QVERIFY(writeFile(m_src + "/big.bin", QByteArray(6 * 1024 * 1024, 'x')));
        JobOptions j = job();
        j.copy.throughputLimitMBps = 1; // slow enough to cancel mid-file
        SyncControl control;
        SyncCallbacks cb;
        cb.progress = [&control](const ProgressInfo &info) {
            if (info.phase == SyncPhase::Copying && info.bytesDone > 0)
                control.cancel();
        };
        const RunReport r = SyncEngine(j, control, cb).run();
        QVERIFY(r.status == RunStatus::Cancelled);
        QVERIFY(!QFile::exists(m_dst + "/big.bin"));
        QVERIFY(tempFiles().isEmpty());
    }

    void logAndReportAreWritten()
    {
        QVERIFY(writeFile(m_src + "/a.mp3", "a"));
        const RunReport r = run(job());
        QVERIFY(QFile::exists(r.logFile));
        QString json = r.logFile;
        json.chop(4);
        QVERIFY(QFile::exists(json + ".json"));
        QVERIFY(readFile(r.logFile).contains("COPIED"));
    }

#ifdef Q_OS_WIN
    void lockedFileIsRetriedThenReported()
    {
        QVERIFY(writeFile(m_src + "/locked.mp3", "locked"));
        QVERIFY(writeFile(m_src + "/free.mp3", "free"));
        const std::wstring path = QDir::toNativeSeparators(m_src + "/locked.mp3").toStdWString();
        HANDLE h = CreateFileW(path.c_str(), GENERIC_READ, 0 /* no sharing */, nullptr,
                               OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
        QVERIFY(h != INVALID_HANDLE_VALUE);
        JobOptions j = job();
        j.retry = {2, 10, 50};
        const RunReport r = run(j);
        CloseHandle(h);
        QVERIFY(r.status == RunStatus::CompletedWithErrors);
        QCOMPARE(r.stats.copied, 1);
        QCOMPARE(r.stats.errors, 1);
        const ReportItem error = *std::find_if(r.items.cbegin(), r.items.cend(),
            [](const ReportItem &i) { return i.kind == ItemKind::Error; });
        QCOMPARE(error.relPath, QString("locked.mp3"));
        QCOMPARE(error.attempts, 3);
        QVERIFY(tempFiles().isEmpty());
    }

    void caseOnlyDifferenceIsNotADeletion()
    {
        QVERIFY(writeFile(m_src + "/Song.mp3", "same"));
        QVERIFY(writeFile(m_dst + "/song.mp3", "same"));
        const RunReport r = run(job(SyncMode::Mirror));
        QCOMPARE(r.stats.deleted, 0);
        QCOMPARE(r.stats.unchanged, 1);
        QVERIFY(QFile::exists(m_dst + "/song.mp3"));
    }
#endif
};

QTEST_GUILESS_MAIN(TestSyncEngine)
#include "tst_syncengine.moc"
