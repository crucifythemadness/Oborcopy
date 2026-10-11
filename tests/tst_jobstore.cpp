#include "AppPaths.h"
#include "AppSettings.h"
#include "JobJson.h"
#include "JobStore.h"

#include <QTemporaryDir>
#include <QTest>

using namespace obor;

class TestJobStore : public QObject
{
    Q_OBJECT

private slots:
    void jsonRoundTrip()
    {
        JobOptions job = JobOptions::defaults("Music Backup");
        job.source = "D:/Music";
        job.destination = "E:/Backup/Music";
        job.mode = SyncMode::Mirror;
        job.filter.excludeFiles = {"*.tmp"};
        job.filter.maxSize = 123456789012LL;
        job.copy.parallelCopies = 4;
        job.copy.verifyAfterCopy = true;
        job.compare.toleranceMode = ToleranceMode::Custom;
        job.compare.toleranceMs = 3000;
        job.compare.dstTolerance = true;
        job.retry.retries = 7;
        job.safety.deleteMode = DeleteMode::Archive;
        job.log.level = LogLevel::AllFiles;

        const JobOptions back = JobJson::fromJson(JobJson::toJson(job));
        QCOMPARE(back.name, job.name);
        QCOMPARE(back.source, job.source);
        QCOMPARE(back.destination, job.destination);
        QVERIFY(back.mode == SyncMode::Mirror);
        QCOMPARE(back.filter.excludeFiles, job.filter.excludeFiles);
        QCOMPARE(back.filter.excludeDirs, job.filter.excludeDirs);
        QCOMPARE(back.filter.maxSize, job.filter.maxSize);
        QCOMPARE(back.copy.parallelCopies, 4);
        QVERIFY(back.copy.verifyAfterCopy);
        QVERIFY(back.compare.toleranceMode == ToleranceMode::Custom);
        QCOMPARE(back.compare.toleranceMs, 3000);
        QVERIFY(back.compare.dstTolerance);
        QCOMPARE(back.retry.retries, 7);
        QVERIFY(back.safety.deleteMode == DeleteMode::Archive);
        QVERIFY(back.log.level == LogLevel::AllFiles);
    }

    void missingKeysUseDefaults()
    {
        const JobOptions job = JobJson::fromJson(QJsonObject{{"name", "x"}});
        QVERIFY(job.mode == SyncMode::Update);
        QCOMPARE(job.retry.retries, 3);
        QVERIFY(job.metadata.fileTimes);
        QVERIFY(job.filter.excludeDirs.contains("System Volume Information"));
    }

    void storeSaveLoadRenameRemove()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const JobStore store(dir.path() + "/jobs");
        JobOptions job = JobOptions::defaults("Photos: NAS/Backup");
        job.source = "C:/Photos";
        QVERIFY(store.save(job));
        QCOMPARE(store.jobNames(), QStringList{"Photos: NAS/Backup"});

        const auto loaded = store.load("photos: nas/backup");
        QVERIFY(loaded.has_value());
        QCOMPARE(loaded->source, QString("C:/Photos"));

        QVERIFY(store.rename("Photos: NAS/Backup", "Pictures"));
        QCOMPARE(store.jobNames(), QStringList{"Pictures"});
        QVERIFY(store.remove("Pictures"));
        QVERIFY(store.jobNames().isEmpty());
    }

    void sanitizeFileName()
    {
        QCOMPARE(AppPaths::sanitizeFileName("a<b>c:d"), QString("a_b_c_d"));
        QCOMPARE(AppPaths::sanitizeFileName("  "), QString("job"));
        QCOMPARE(AppPaths::sanitizeFileName("name."), QString("name"));
    }

    void appSettingsRoundTrip()
    {
        QTemporaryDir dir;
        AppSettings s;
        s.theme = "dark";
        s.lastJob = "Music";
        s.windowGeometry = QByteArray("\x01\x02\x03", 3);
        QVERIFY(s.save(dir.path() + "/settings.json"));
        const AppSettings back = AppSettings::load(dir.path() + "/settings.json");
        QCOMPARE(back.theme, QString("dark"));
        QCOMPARE(back.lastJob, QString("Music"));
        QCOMPARE(back.windowGeometry, s.windowGeometry);
    }
};

QTEST_GUILESS_MAIN(TestJobStore)
#include "tst_jobstore.moc"
