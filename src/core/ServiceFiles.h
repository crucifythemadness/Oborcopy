#pragma once

#include <QDateTime>
#include <QJsonObject>
#include <QMap>
#include <QString>
#include <QVector>

// Names, file locations and the small JSON files the GUI and the scheduler
// service share. Both run as the same user, so they resolve the same
// AppPaths::serviceDir():
//   schedule.json  written by the GUI   - which jobs run when
//   state.json     written by the service - last run / anchor per job
//   status.json    written by the service - what it is doing right now (+ heartbeat)
//   service.log    written by the service - start/stop/run lines, size rotated
namespace obor::ServiceInfo {

QString serviceName();     // Windows service name: "Oborcopy"
QString displayName();     // "Oborcopy Scheduler"
QString description();
QString unitName();        // systemd user unit: "oborcopy.service"
QString serviceExecutable(); // "oborcopy-service(.exe)" next to the GUI
QString cliExecutable();     // "oborcopy-cli(.exe)" next to the service

QString schedulePath();
QString statePath();
QString statusPath();
QString logPath();

// Seconds after which a missing heartbeat means the service is not responding.
constexpr int kHeartbeatSeconds = 30;
constexpr int kStaleSeconds = 90;

} // namespace obor::ServiceInfo

namespace obor {

// What the service remembers about one scheduled job.
struct JobRunState
{
    QDateTime anchor;      // next run is computed after this time (last run end or schedule change)
    QString fingerprint;   // ScheduleEntry::fingerprint() the anchor belongs to
    QDateTime lastStart;
    QDateTime lastRun;     // end of the last run
    QString lastStatus;    // "Completed", "Completed with errors", "Cancelled", "Failed", ...
    QString lastReport;    // .json run report (opens the summary)
    int lastExitCode = -1;

    QJsonObject toJson() const;
    static JobRunState fromJson(const QJsonObject &object);
};

struct ServiceRunState
{
    QMap<QString, JobRunState> jobs; // key: job name

    static ServiceRunState load(const QString &path);
    bool save(const QString &path) const;
};

enum class ServiceActivity { Stopped, Idle, Busy, Paused, Stopping };

QString toString(ServiceActivity activity);

struct NextRun
{
    QString job;
    QDateTime at;
};

struct ServiceStatus
{
    ServiceActivity activity = ServiceActivity::Stopped;
    qint64 pid = 0;
    QString version;
    QString binary;
    QDateTime started;
    QDateTime heartbeat;
    QString currentJob;
    QDateTime currentSince;
    QVector<NextRun> nextRuns; // sorted, enabled entries only
    QString lastError;

    bool isFresh(const QDateTime &now = QDateTime::currentDateTime()) const;

    QJsonObject toJson() const;
    static ServiceStatus fromJson(const QJsonObject &object);
    static ServiceStatus load(const QString &path, bool *ok = nullptr);
    bool save(const QString &path) const;
};

} // namespace obor
