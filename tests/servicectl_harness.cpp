// End-to-end test of the Linux scheduler service: drives the real
// ServiceController (the code behind the Service Manager buttons) against a
// real systemd user session. Not part of ctest - it needs systemd; run it with
// scripts/test-linux-service.ps1, which starts a systemd container.

#include "AppInfo.h"
#include "AppPaths.h"
#include "JobStore.h"
#include "Schedule.h"
#include "ServiceController.h"
#include "ServiceFiles.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QEventLoop>
#include <QFile>
#include <QTimer>

#include <cstdio>
#include <functional>

using namespace obor;

namespace {

int g_failures = 0;

void report(bool ok, const QString &step, const QString &detail = QString())
{
    std::printf("[%s] %s%s\n", ok ? "PASS" : "FAIL", qPrintable(step),
                detail.isEmpty() ? "" : qPrintable(QStringLiteral(" – ") + detail));
    std::fflush(stdout);
    if (!ok)
        ++g_failures;
}

using Predicate = std::function<bool()>;

// Runs the event loop until 'done' is true or the timeout expires.
bool waitUntil(ServiceController &controller, const Predicate &done, int timeoutMs)
{
    if (done())
        return true;
    QEventLoop loop;
    QTimer timeout;
    timeout.setSingleShot(true);
    QObject::connect(&timeout, &QTimer::timeout, &loop, &QEventLoop::quit);
    QTimer poll; // state.json / service.log changes do not emit signals
    QObject::connect(&poll, &QTimer::timeout, &loop, [&]() {
        if (done())
            loop.quit();
    });
    QObject::connect(&controller, &ServiceController::stateChanged, &loop, [&]() {
        if (done())
            loop.quit();
    });
    timeout.start(timeoutMs);
    poll.start(1000);
    loop.exec();
    return done();
}

bool isAlive(ServiceController &c)
{
    const auto kind = c.state().kind;
    return !c.isBusy() && c.state().responding
           && (kind == ServiceState::Kind::Running || kind == ServiceState::Kind::Busy);
}

bool isKind(ServiceController &c, ServiceState::Kind kind)
{
    return !c.isBusy() && c.state().kind == kind;
}

// Starts an action and waits for its actionFinished signal.
bool runAction(ServiceController &c, const std::function<void()> &action, QString *message, int timeoutMs)
{
    bool finished = false;
    bool ok = false;
    const auto connection = QObject::connect(&c, &ServiceController::actionFinished,
                                             [&](bool success, const QString &text) {
                                                 finished = true;
                                                 ok = success;
                                                 *message = text;
                                             });
    action();
    waitUntil(c, [&]() { return finished; }, timeoutMs);
    QObject::disconnect(connection);
    if (!finished)
        *message = QStringLiteral("no answer within %1 s").arg(timeoutMs / 1000);
    return finished && ok;
}

QString lastStatus(const QString &job)
{
    return ServiceRunState::load(ServiceInfo::statePath()).jobs.value(job).lastStatus;
}

QString serviceLog()
{
    QFile file(ServiceInfo::logPath());
    return file.open(QIODevice::ReadOnly) ? QString::fromUtf8(file.readAll()) : QString();
}

// Next full minute that is at least 20 s away.
QTime nextMinute()
{
    const QDateTime t = QDateTime::currentDateTime().addSecs(80);
    return QTime(t.time().hour(), t.time().minute());
}

void writeJob(const QString &name, const QString &source, const QString &destination)
{
    JobOptions job = JobOptions::defaults(name);
    job.source = source;
    job.destination = destination;
    JobStore(AppPaths::jobsDir()).save(job);
}

} // namespace

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    QCoreApplication::setApplicationName(AppInfo::name()); // same folders as the GUI
    std::printf("Oborcopy %s – Linux service test, program %s\n", qPrintable(AppInfo::version()),
                qPrintable(ServiceController::serviceBinary()));

    ServiceController c;
    QString message;

    waitUntil(c, [&]() { return c.state().kind != ServiceState::Kind::Unknown; }, 15000);
    report(c.state().kind != ServiceState::Kind::Unavailable, QStringLiteral("systemd user session available"),
           c.state().message);
    if (c.state().kind == ServiceState::Kind::Unavailable)
        return 1;
    if (c.state().isInstalled())
        runAction(c, [&]() { c.uninstall(); }, &message, 90000); // leftover from an earlier run

    report(runAction(c, [&]() { c.install(); }, &message, 90000), QStringLiteral("install"), message);
    report(waitUntil(c, [&]() { return isAlive(c); }, 30000),
           QStringLiteral("service running and reporting"), c.state().title());

    // A scheduled job runs and is recorded.
    const QString base = QDir::homePath() + QStringLiteral("/oborcopy-test");
    QDir().mkpath(base + QStringLiteral("/src/sub"));
    QFile file(base + QStringLiteral("/src/sub/hello.txt"));
    if (file.open(QIODevice::WriteOnly))
        file.write("hello from the scheduler test\n");
    file.close();
    writeJob(QStringLiteral("Scheduled"), base + QStringLiteral("/src"), base + QStringLiteral("/dst"));
    writeJob(QStringLiteral("RetryTest"), base + QStringLiteral("/missing"), base + QStringLiteral("/dst2"));
    ServiceSchedule schedule;
    ScheduleEntry entry;
    entry.job = QStringLiteral("Scheduled");
    entry.frequency = Frequency::Daily;
    entry.time = nextMinute();
    schedule.entries << entry;
    ScheduleEntry retry = entry;
    retry.job = QStringLiteral("RetryTest");
    retry.retryEnabled = true;
    retry.retryCount = 1;
    retry.retryIntervalMinutes = 1;
    schedule.entries << retry;
    schedule.save(ServiceInfo::schedulePath());
    std::printf("       jobs scheduled for %s, waiting…\n", qPrintable(entry.time.toString(QStringLiteral("HH:mm"))));
    std::fflush(stdout);
    report(waitUntil(c, [&]() { return lastStatus(QStringLiteral("Scheduled")) == QLatin1String("Completed"); }, 150000)
               && QFile::exists(base + QStringLiteral("/dst/sub/hello.txt")),
           QStringLiteral("scheduled job ran and copied the file"), lastStatus(QStringLiteral("Scheduled")));
    report(waitUntil(c, [&]() { return serviceLog().contains(QLatin1String("No more retries for 'RetryTest'")); }, 150000),
           QStringLiteral("failed job was retried once, then gave up"));

    // Runs one control action and waits for the expected state; the reported
    // detail is the state reached afterwards.
    auto step = [&](const QString &name, const std::function<void()> &action, const Predicate &reached,
                    int timeoutMs) {
        const bool ok = runAction(c, action, &message, timeoutMs) && waitUntil(c, reached, 30000);
        report(ok, name, ok ? c.state().title() : c.state().title() + QStringLiteral(" / ") + message);
    };
    step(QStringLiteral("pause"), [&]() { c.pause(); },
         [&]() { return isKind(c, ServiceState::Kind::Paused); }, 15000);
    step(QStringLiteral("resume"), [&]() { c.resume(); },
         [&]() { return isKind(c, ServiceState::Kind::Running); }, 15000);
    step(QStringLiteral("stop"), [&]() { c.stop(); },
         [&]() { return isKind(c, ServiceState::Kind::Stopped); }, 90000);
    step(QStringLiteral("start"), [&]() { c.start(); }, [&]() { return isAlive(c); }, 30000);
    const QDateTime startedBefore = c.state().status.started;
    step(QStringLiteral("restart"), [&]() { c.restart(); },
         [&]() { return isAlive(c) && c.state().status.started > startedBefore; }, 90000);
    step(QStringLiteral("uninstall"), [&]() { c.uninstall(); },
         [&]() { return isKind(c, ServiceState::Kind::NotInstalled); }, 90000);

    std::printf("\n%s – %d failure(s)\n", g_failures == 0 ? "ALL PASSED" : "FAILED", g_failures);
    return g_failures == 0 ? 0 : 1;
}
