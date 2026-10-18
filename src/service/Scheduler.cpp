#include "Scheduler.h"

#include "AppInfo.h"

#include <QCoreApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QFileInfo>
#include <QProcess>
#include <QThread>

#include <algorithm>

namespace obor {

namespace {

constexpr int kGraceSeconds = 15 * 60;   // a run this late is still "on time" (catch-up off)
constexpr int kCancelTimeoutMs = 30000;  // after "cancel", wait this long before killing the CLI
constexpr int kMaxNextRuns = 10;
constexpr int kMaxErrorLines = 20;

#ifdef Q_OS_WIN
constexpr unsigned long kCreateNoWindow = 0x08000000; // CREATE_NO_WINDOW
#endif

} // namespace

Scheduler::Scheduler()
    : m_log(ServiceInfo::logPath())
    , m_cli(QDir(QCoreApplication::applicationDirPath()).filePath(ServiceInfo::cliExecutable()))
{
}

void Scheduler::run()
{
    m_status = ServiceStatus();
    m_status.pid = QCoreApplication::applicationPid();
    m_status.version = AppInfo::version();
    m_status.binary = QDir::toNativeSeparators(QCoreApplication::applicationFilePath());
    m_status.started = QDateTime::currentDateTime();
    m_log.info(QStringLiteral("%1 %2 started (pid %3)")
                   .arg(ServiceInfo::displayName(), AppInfo::version())
                   .arg(m_status.pid));
    if (!QFileInfo::exists(m_cli))
        m_log.error(QStringLiteral("Command line tool not found: %1").arg(m_cli));
    m_state = ServiceRunState::load(ServiceInfo::statePath());
    reloadSchedule(true);

    while (!m_stop) {
        reloadSchedule(m_reload.exchange(false));
        const QDateTime now = QDateTime::currentDateTime();
        if (!m_paused) {
            if (const auto index = dueEntry(now)) {
                const ScheduleEntry entry = m_schedule.entries.at(*index);
                runEntry(entry, nextDue(entry));
                continue;
            }
        }
        writeStatus(false);
        sleepUntilNextCheck(now);
    }

    m_status.activity = ServiceActivity::Stopped;
    m_status.currentJob.clear();
    m_status.currentSince = {};
    m_status.heartbeat = QDateTime::currentDateTime();
    m_status.save(ServiceInfo::statusPath());
    m_log.info(QStringLiteral("%1 stopped").arg(ServiceInfo::displayName()));
}

void Scheduler::reloadSchedule(bool force)
{
    const QFileInfo info(ServiceInfo::schedulePath());
    const QDateTime modified = info.exists() ? info.lastModified() : QDateTime();
    const qint64 size = info.exists() ? info.size() : -1;
    if (!force && modified == m_scheduleModified && size == m_scheduleSize)
        return;
    m_scheduleModified = modified;
    m_scheduleSize = size;
    QString error;
    m_schedule = ServiceSchedule::load(info.filePath(), &error);
    m_status.lastError = error;
    if (!error.isEmpty())
        m_log.error(error);
    reconcile();
    int enabled = 0;
    for (const ScheduleEntry &entry : std::as_const(m_schedule.entries))
        enabled += entry.enabled ? 1 : 0;
    m_log.info(QStringLiteral("Schedule loaded: %1 job(s), %2 enabled").arg(m_schedule.entries.size()).arg(enabled));
    writeStatus(true);
}

// Gives new or changed entries a fresh anchor (= now), so a schedule edit never
// triggers an immediate "catch-up" run, and forgets jobs no longer scheduled.
void Scheduler::reconcile()
{
    const QDateTime now = QDateTime::currentDateTime();
    QMap<QString, JobRunState> jobs;
    for (const ScheduleEntry &entry : std::as_const(m_schedule.entries)) {
        JobRunState state = m_state.jobs.value(entry.job);
        const QString fingerprint = entry.fingerprint();
        if (!state.anchor.isValid() || state.fingerprint != fingerprint) {
            state.anchor = now;
            state.fingerprint = fingerprint;
            state.retryAt = {};
            state.retryAttempt = 0;
        }
        jobs.insert(entry.job, state);
    }
    m_state.jobs = jobs;
    saveState();
}

Scheduler::Due Scheduler::nextDue(const ScheduleEntry &entry) const
{
    const JobRunState state = m_state.jobs.value(entry.job);
    const QDateTime regular = entry.nextRun(state.anchor);
    if (entry.retryEnabled && state.retryAt.isValid() && (!regular.isValid() || state.retryAt < regular))
        return {state.retryAt, state.retryAttempt};
    return {regular, 0};
}

std::optional<int> Scheduler::dueEntry(const QDateTime &now) const
{
    std::optional<int> best;
    QDateTime bestDue;
    for (int i = 0; i < m_schedule.entries.size(); ++i) {
        const ScheduleEntry &entry = m_schedule.entries.at(i);
        if (!entry.enabled)
            continue;
        const QDateTime due = nextDue(entry).at;
        if (due.isValid() && due <= now && (!best || due < bestDue)) {
            best = i;
            bestDue = due;
        }
    }
    return best;
}

void Scheduler::runEntry(const ScheduleEntry &entry, const Due &due)
{
    JobRunState &state = m_state.jobs[entry.job];
    const QDateTime now = QDateTime::currentDateTime();
    if (due.attempt == 0 && !entry.catchUp && due.at.secsTo(now) > kGraceSeconds) {
        m_log.info(QStringLiteral("Skipped missed run of '%1' (was due %2, catch-up is off)")
                       .arg(entry.job, due.at.toString(Qt::ISODate)));
        state.anchor = now;
        saveState();
        return;
    }

    m_log.info(QStringLiteral("Starting '%1' (due %2%3)")
                   .arg(entry.job, due.at.toString(Qt::ISODate),
                        due.attempt > 0 ? QStringLiteral(", retry %1 of %2").arg(due.attempt).arg(entry.retryCount)
                                        : QString()));
    state.lastStart = now;
    m_busyJob = entry.job;
    m_status.currentJob = entry.job;
    m_status.currentSince = now;
    writeStatus(true);

    const Result result = runCli(entry);

    JobRunState &done = m_state.jobs[entry.job]; // re-fetch: the map may have changed
    done.lastRun = QDateTime::currentDateTime();
    done.anchor = done.lastRun;
    done.lastStatus = result.status;
    done.lastExitCode = result.exitCode;
    done.lastReport.clear();
    if (result.logFile.endsWith(QLatin1String(".log"))) {
        done.lastReport = result.logFile;
        done.lastReport.chop(4);
        done.lastReport += QStringLiteral(".json");
    }
    scheduleRetry(entry, due, result.status);
    saveState();
    m_busyJob.clear();
    m_status.currentJob.clear();
    m_status.currentSince = {};
    m_log.info(QStringLiteral("Finished '%1': %2 (exit code %3)%4")
                   .arg(entry.job, result.status)
                   .arg(result.exitCode)
                   .arg(result.logFile.isEmpty() ? QString() : QStringLiteral(", log: ") + result.logFile));
    writeStatus(true);
}

// A failed run (e.g. backup disk not connected yet) is retried if the entry
// allows it. Runs that completed - even with errors - or were cancelled (deletion
// threshold, service stop) are not retried: that would bypass a safety stop.
void Scheduler::scheduleRetry(const ScheduleEntry &entry, const Due &due, const QString &status)
{
    JobRunState &state = m_state.jobs[entry.job];
    const bool failed = status == QLatin1String("Failed") || status == QLatin1String("Failed to start")
                        || status == QLatin1String("Crashed");
    state.retryAt = {};
    if (!failed || !entry.retryEnabled || m_stop) {
        state.retryAttempt = 0;
        return;
    }
    const int done = due.attempt; // retries already made before this run
    if (done >= entry.retryCount) {
        m_log.info(QStringLiteral("No more retries for '%1' (%2 of %2 used)").arg(entry.job).arg(entry.retryCount));
        state.retryAttempt = 0;
        return;
    }
    state.retryAttempt = done + 1;
    state.retryAt = QDateTime::currentDateTime().addSecs(qint64(entry.retryIntervalMinutes) * 60);
    m_log.info(QStringLiteral("Retry %1 of %2 for '%3' at %4")
                   .arg(state.retryAttempt)
                   .arg(entry.retryCount)
                   .arg(entry.job, state.retryAt.toString(QStringLiteral("HH:mm"))));
}

Scheduler::Result Scheduler::runCli(const ScheduleEntry &entry)
{
    QStringList args{QStringLiteral("--job"), entry.job, QStringLiteral("--quiet"),
                     QStringLiteral("--scheduled"), QStringLiteral("--stdin-control")};
    if (entry.allowDeletions)
        args << QStringLiteral("--yes");

    QProcess process;
    process.setProgram(m_cli);
    process.setArguments(args);
#ifdef Q_OS_WIN
    process.setCreateProcessArgumentsModifier([](QProcess::CreateProcessArguments *a) {
        a->flags |= kCreateNoWindow;
    });
#endif
    process.start();
    Result result;
    if (!process.waitForStarted(30000)) {
        result.status = QStringLiteral("Failed to start");
        m_log.error(QStringLiteral("Cannot start %1: %2").arg(m_cli, process.errorString()));
        return result;
    }

    bool cancelSent = false;
    QElapsedTimer cancelTimer;
    while (!process.waitForFinished(1000)) {
        if (process.state() == QProcess::NotRunning)
            break;
        if (m_stop && !cancelSent) {
            m_log.info(QStringLiteral("Service is stopping: cancelling '%1'").arg(entry.job));
            process.write("cancel\n");
            process.closeWriteChannel();
            cancelSent = true;
            cancelTimer.start();
        }
        if (cancelSent && cancelTimer.elapsed() > kCancelTimeoutMs) {
            m_log.error(QStringLiteral("'%1' did not stop in time and was terminated").arg(entry.job));
            process.kill();
            process.waitForFinished(5000);
            break;
        }
        writeStatus(false);
    }

    const QStringList out = QString::fromUtf8(process.readAllStandardOutput()).split(QLatin1Char('\n'));
    for (const QString &raw : out) {
        const QString line = raw.trimmed();
        if (line.startsWith(QLatin1String("Status: ")))
            result.status = line.mid(8);
        else if (line.startsWith(QLatin1String("Log: ")))
            result.logFile = line.mid(5);
    }
    const QStringList err = QString::fromUtf8(process.readAllStandardError()).split(QLatin1Char('\n'), Qt::SkipEmptyParts);
    for (int i = 0; i < std::min<int>(err.size(), kMaxErrorLines); ++i)
        m_log.error(QStringLiteral("  %1: %2").arg(entry.job, err.at(i).trimmed()));
    if (err.size() > kMaxErrorLines)
        m_log.error(QStringLiteral("  %1: ... %2 more line(s), see the job log").arg(entry.job).arg(err.size() - kMaxErrorLines));

    if (process.exitStatus() == QProcess::CrashExit) {
        result.status = cancelSent ? QStringLiteral("Cancelled") : QStringLiteral("Crashed");
    } else {
        result.exitCode = process.exitCode();
        if (result.status.isEmpty())
            result.status = QStringLiteral("Failed");
    }
    return result;
}

void Scheduler::saveState()
{
    if (!m_state.save(ServiceInfo::statePath()))
        m_log.error(QStringLiteral("Cannot write %1").arg(ServiceInfo::statePath()));
}

void Scheduler::writeStatus(bool force)
{
    const QDateTime now = QDateTime::currentDateTime();
    ServiceActivity activity = ServiceActivity::Idle;
    if (m_stop)
        activity = ServiceActivity::Stopping;
    else if (!m_busyJob.isEmpty())
        activity = ServiceActivity::Busy;
    else if (m_paused)
        activity = ServiceActivity::Paused;

    if (!force && activity == m_writtenActivity && m_statusWritten.isValid()
        && m_statusWritten.secsTo(now) < ServiceInfo::kHeartbeatSeconds)
        return;

    m_status.activity = activity;
    m_status.heartbeat = now;
    m_status.nextRuns.clear();
    for (const ScheduleEntry &entry : std::as_const(m_schedule.entries)) {
        if (!entry.enabled)
            continue;
        const Due due = nextDue(entry);
        if (due.at.isValid())
            m_status.nextRuns.append({entry.job, due.at, due.attempt});
    }
    std::sort(m_status.nextRuns.begin(), m_status.nextRuns.end(),
              [](const NextRun &a, const NextRun &b) { return a.at < b.at; });
    if (m_status.nextRuns.size() > kMaxNextRuns)
        m_status.nextRuns.resize(kMaxNextRuns);
    if (m_status.save(ServiceInfo::statusPath())) {
        m_statusWritten = now;
        m_writtenActivity = activity;
    }
}

void Scheduler::sleepUntilNextCheck(const QDateTime &now)
{
    // Wake at least every 5 s (schedule changes) and right when a job is due;
    // sleep in 250 ms slices so stop/pause requests are answered quickly.
    qint64 waitMs = 5000;
    if (!m_paused) {
        for (const ScheduleEntry &entry : std::as_const(m_schedule.entries)) {
            const QDateTime due = entry.enabled ? nextDue(entry).at : QDateTime();
            if (due.isValid())
                waitMs = std::min(waitMs, std::max<qint64>(0, now.msecsTo(due)) + 50);
        }
    }
    QElapsedTimer timer;
    timer.start();
    const bool wasPaused = m_paused;
    while (timer.elapsed() < waitMs && !m_stop && !m_reload && m_paused == wasPaused)
        QThread::msleep(250);
}

} // namespace obor
