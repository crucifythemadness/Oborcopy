#pragma once

#include "Schedule.h"
#include "ServiceFiles.h"
#include "ServiceLog.h"

#include <QDateTime>
#include <QString>

#include <atomic>
#include <optional>

namespace obor {

// The heart of the service: waits until a scheduled job is due and runs it
// with oborcopy-cli, one job at a time. Blocking; runs on the service thread.
// Control requests only set atomic flags (safe from signal handlers and the
// Windows control handler); the loop checks them at least once per second.
class Scheduler
{
public:
    Scheduler();

    void run();

    void requestStop() { m_stop = true; }
    void setPaused(bool paused) { m_paused = paused; }
    void requestReload() { m_reload = true; }

private:
    struct Result
    {
        QString status;
        QString logFile;
        int exitCode = -1;
    };

    struct Due
    {
        QDateTime at;
        int attempt = 0; // 0 = regular run, n = retry n
    };

    void reloadSchedule(bool force);
    void reconcile();
    // Regular next run or a pending retry, whichever comes first.
    Due nextDue(const ScheduleEntry &entry) const;
    std::optional<int> dueEntry(const QDateTime &now) const;
    void runEntry(const ScheduleEntry &entry, const Due &due);
    void scheduleRetry(const ScheduleEntry &entry, const Due &due, const QString &status);
    Result runCli(const ScheduleEntry &entry);
    void saveState();
    void writeStatus(bool force);
    void sleepUntilNextCheck(const QDateTime &now);

    ServiceLog m_log;
    QString m_cli;
    ServiceSchedule m_schedule;
    QDateTime m_scheduleModified;
    qint64 m_scheduleSize = -1;
    ServiceRunState m_state;
    ServiceStatus m_status;
    QDateTime m_statusWritten;
    ServiceActivity m_writtenActivity = ServiceActivity::Stopped;
    QString m_busyJob;

    std::atomic_bool m_stop{false};
    std::atomic_bool m_paused{false};
    std::atomic_bool m_reload{false};
};

} // namespace obor
