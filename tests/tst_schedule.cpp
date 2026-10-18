#include "RunReport.h"
#include "Schedule.h"
#include "ServiceFiles.h"

#include <QTemporaryDir>
#include <QTest>

using namespace obor;

namespace {

QDateTime at(int y, int m, int d, int h, int min)
{
    return QDateTime(QDate(y, m, d), QTime(h, min));
}

ScheduleEntry entry(Frequency frequency, QTime time)
{
    ScheduleEntry e;
    e.job = QStringLiteral("Music Backup");
    e.frequency = frequency;
    e.time = time;
    return e;
}

} // namespace

class TestSchedule : public QObject
{
    Q_OBJECT

private slots:
    void daily()
    {
        const ScheduleEntry e = entry(Frequency::Daily, QTime(23, 30));
        QCOMPARE(e.nextRun(at(2026, 10, 15, 10, 0)), at(2026, 10, 15, 23, 30));
        // Strictly after: a run that just happened is not due again.
        QCOMPARE(e.nextRun(at(2026, 10, 15, 23, 30)), at(2026, 10, 16, 23, 30));
        QCOMPARE(e.nextRun(at(2026, 12, 31, 23, 45)), at(2027, 1, 1, 23, 30));
    }

    void hourly()
    {
        ScheduleEntry e = entry(Frequency::Hourly, QTime(0, 15));
        e.intervalHours = 4;
        QCOMPARE(e.nextRun(at(2026, 10, 15, 5, 0)), at(2026, 10, 15, 8, 15));
        QCOMPARE(e.nextRun(at(2026, 10, 15, 20, 15)), at(2026, 10, 16, 0, 15));
        e.intervalHours = 1;
        QCOMPARE(e.nextRun(at(2026, 10, 15, 5, 15)), at(2026, 10, 15, 6, 15));
        QCOMPARE(e.describe(), QStringLiteral("Every hour at :15"));
    }

    void weekly()
    {
        ScheduleEntry e = entry(Frequency::Weekly, QTime(7, 0));
        e.weekdays = {1, 4}; // Monday, Thursday
        QCOMPARE(e.nextRun(at(2026, 10, 14, 12, 0)), at(2026, 10, 15, 7, 0)); // Wed -> Thu
        QCOMPARE(e.nextRun(at(2026, 10, 15, 7, 0)), at(2026, 10, 19, 7, 0));  // Thu -> Mon
        QCOMPARE(e.describe(), QStringLiteral("Weekly on Mon, Thu at 07:00"));
        e.weekdays.clear();
        QVERIFY(!e.nextRun(at(2026, 10, 14, 12, 0)).isValid());
    }

    void monthlyClampsToMonthEnd()
    {
        ScheduleEntry e = entry(Frequency::Monthly, QTime(2, 0));
        e.dayOfMonth = 31;
        QCOMPARE(e.nextRun(at(2026, 11, 1, 0, 0)), at(2026, 11, 30, 2, 0));
        QCOMPARE(e.nextRun(at(2026, 2, 1, 0, 0)), at(2026, 2, 28, 2, 0));
        QCOMPARE(e.nextRun(at(2028, 2, 1, 0, 0)), at(2028, 2, 29, 2, 0)); // leap year
        e.dayOfMonth = 0; // last day
        QCOMPARE(e.nextRun(at(2026, 4, 30, 3, 0)), at(2026, 5, 31, 2, 0));
        QCOMPARE(e.describe(), QStringLiteral("Monthly on the last day at 02:00"));
        e.dayOfMonth = 15;
        QCOMPARE(e.nextRun(at(2026, 10, 15, 2, 0)), at(2026, 11, 15, 2, 0));
    }

    void nextRunAlwaysAdvances()
    {
        // Over a whole year (incl. both DST switches of the local zone) every
        // next run lies after the previous one.
        ScheduleEntry e = entry(Frequency::Hourly, QTime(0, 30));
        QDateTime t = at(2026, 1, 1, 0, 0);
        for (int i = 0; i < 24 * 366; ++i) {
            const QDateTime next = e.nextRun(t);
            QVERIFY2(next.isValid() && next > t, qPrintable(t.toString(Qt::ISODate)));
            t = next;
        }
        QVERIFY(t.date().year() >= 2027);
    }

    void jsonAndFingerprint()
    {
        ScheduleEntry e = entry(Frequency::Weekly, QTime(21, 5));
        e.weekdays = {6, 7};
        e.catchUp = false;
        e.allowDeletions = false;
        const ScheduleEntry back = ScheduleEntry::fromJson(e.toJson());
        QCOMPARE(back.job, e.job);
        QVERIFY(back.frequency == Frequency::Weekly);
        QCOMPARE(back.time, e.time);
        QCOMPARE(back.weekdays, e.weekdays);
        QCOMPARE(back.catchUp, false);
        QCOMPARE(back.allowDeletions, false);
        QCOMPARE(back.fingerprint(), e.fingerprint());

        ScheduleEntry changed = e;
        changed.allowDeletions = true; // not part of the timing
        QCOMPARE(changed.fingerprint(), e.fingerprint());
        changed.time = QTime(21, 6);
        QVERIFY(changed.fingerprint() != e.fingerprint());
        changed = e;
        changed.enabled = false;
        QVERIFY(changed.fingerprint() != e.fingerprint());
    }

    void retryOptions()
    {
        const ScheduleEntry defaults;
        QVERIFY(!defaults.retryEnabled);
        QCOMPARE(defaults.retryCount, 8);
        QCOMPARE(defaults.retryIntervalMinutes, 15);

        ScheduleEntry e = entry(Frequency::Daily, QTime(23, 30));
        e.retryEnabled = true;
        e.retryCount = 3;
        e.retryIntervalMinutes = 5;
        const ScheduleEntry back = ScheduleEntry::fromJson(e.toJson());
        QVERIFY(back.retryEnabled);
        QCOMPARE(back.retryCount, 3);
        QCOMPARE(back.retryIntervalMinutes, 5);
        // Retry settings are not timing: changing them must not re-anchor the schedule.
        QCOMPARE(back.fingerprint(), entry(Frequency::Daily, QTime(23, 30)).fingerprint());

        QJsonObject wild = e.toJson();
        wild["retryCount"] = 999;
        wild["retryIntervalMinutes"] = 0;
        QCOMPARE(ScheduleEntry::fromJson(wild).retryCount, 24);
        QCOMPARE(ScheduleEntry::fromJson(wild).retryIntervalMinutes, 1);
        // Old schedule files without the keys keep the defaults.
        QVERIFY(!ScheduleEntry::fromJson(QJsonObject{{"job", "Old"}}).retryEnabled);

        JobRunState state;
        state.retryAttempt = 2;
        state.retryAt = at(2026, 10, 18, 8, 15);
        const JobRunState stateBack = JobRunState::fromJson(state.toJson());
        QCOMPARE(stateBack.retryAttempt, 2);
        QCOMPARE(stateBack.retryAt, state.retryAt);

        ServiceStatus status;
        status.nextRuns << NextRun{QStringLiteral("Music"), at(2026, 10, 18, 8, 15), 2};
        QCOMPARE(ServiceStatus::fromJson(status.toJson()).nextRuns.at(0).attempt, 2);
    }

    void scheduleFile()
    {
        QTemporaryDir dir;
        const QString path = dir.filePath(QStringLiteral("service/schedule.json"));
        ServiceSchedule schedule;
        schedule.entries << entry(Frequency::Daily, QTime(23, 30));
        ScheduleEntry photos = entry(Frequency::Monthly, QTime(1, 0));
        photos.job = QStringLiteral("Photos");
        schedule.entries << photos;
        QVERIFY(schedule.renameJob(QStringLiteral("music backup"), QStringLiteral("Music")));
        QVERIFY(schedule.save(path));

        QString error;
        ServiceSchedule back = ServiceSchedule::load(path, &error);
        QVERIFY(error.isEmpty());
        QCOMPARE(back.entries.size(), 2);
        QCOMPARE(back.entries.at(0).job, QStringLiteral("Music"));
        QVERIFY(back.removeJob(QStringLiteral("Photos")));
        QVERIFY(!back.contains(QStringLiteral("Photos")));
        QVERIFY(ServiceSchedule::load(dir.filePath(QStringLiteral("missing.json"))).entries.isEmpty());
    }

    void statusFiles()
    {
        QTemporaryDir dir;
        ServiceStatus status;
        status.activity = ServiceActivity::Busy;
        status.currentJob = QStringLiteral("Music");
        status.heartbeat = QDateTime::currentDateTime();
        status.nextRuns << NextRun{QStringLiteral("Photos"), at(2026, 10, 16, 23, 30)};
        QVERIFY(status.save(dir.filePath(QStringLiteral("status.json"))));
        bool ok = false;
        const ServiceStatus back = ServiceStatus::load(dir.filePath(QStringLiteral("status.json")), &ok);
        QVERIFY(ok);
        QVERIFY(back.activity == ServiceActivity::Busy);
        QCOMPARE(back.currentJob, status.currentJob);
        QCOMPARE(back.nextRuns.size(), 1);
        QCOMPARE(back.nextRuns.at(0).at, at(2026, 10, 16, 23, 30));
        QVERIFY(back.isFresh());
        QVERIFY(!back.isFresh(QDateTime::currentDateTime().addSecs(ServiceInfo::kStaleSeconds + 1)));

        ServiceRunState state;
        state.jobs[QStringLiteral("Music")].lastStatus = QStringLiteral("Completed");
        state.jobs[QStringLiteral("Music")].lastRun = at(2026, 10, 15, 23, 41);
        QVERIFY(state.save(dir.filePath(QStringLiteral("state.json"))));
        const ServiceRunState stateBack = ServiceRunState::load(dir.filePath(QStringLiteral("state.json")));
        QCOMPARE(stateBack.jobs.value(QStringLiteral("Music")).lastRun, at(2026, 10, 15, 23, 41));
    }

    void reportTrigger()
    {
        RunReport report;
        report.scheduled = true;
        QVERIFY(RunReport::fromJson(report.toJson()).scheduled);
        QVERIFY(!RunReport::fromJson(QJsonObject()).scheduled); // old reports: manual
    }
};

QTEST_MAIN(TestSchedule)
#include "tst_schedule.moc"
