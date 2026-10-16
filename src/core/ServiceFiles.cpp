#include "ServiceFiles.h"

#include "AppPaths.h"

#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSaveFile>

#include <optional>

namespace obor::ServiceInfo {

QString serviceName()
{
    return QStringLiteral("Oborcopy");
}

QString displayName()
{
    return QStringLiteral("Oborcopy Scheduler");
}

QString description()
{
    return QStringLiteral("Runs scheduled Oborcopy backup and sync jobs. Managed from the Oborcopy application.");
}

QString unitName()
{
    return QStringLiteral("oborcopy.service");
}

QString serviceExecutable()
{
#ifdef Q_OS_WIN
    return QStringLiteral("oborcopy-service.exe");
#else
    return QStringLiteral("oborcopy-service");
#endif
}

QString cliExecutable()
{
#ifdef Q_OS_WIN
    return QStringLiteral("oborcopy-cli.exe");
#else
    return QStringLiteral("oborcopy-cli");
#endif
}

QString schedulePath()
{
    return AppPaths::serviceDir() + QStringLiteral("/schedule.json");
}

QString statePath()
{
    return AppPaths::serviceDir() + QStringLiteral("/state.json");
}

QString statusPath()
{
    return AppPaths::serviceDir() + QStringLiteral("/status.json");
}

QString logPath()
{
    return AppPaths::serviceDir() + QStringLiteral("/service.log");
}

} // namespace obor::ServiceInfo

namespace obor {

namespace {

const char *const kActivityKeys[] = {"stopped", "idle", "busy", "paused", "stopping"};

QString dateToString(const QDateTime &dt)
{
    return dt.isValid() ? dt.toString(Qt::ISODate) : QString();
}

QDateTime dateFromJson(const QJsonValue &v)
{
    return QDateTime::fromString(v.toString(), Qt::ISODate);
}

std::optional<QJsonObject> readObject(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return std::nullopt;
    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    if (!doc.isObject())
        return std::nullopt;
    return doc.object();
}

bool writeObject(const QString &path, const QJsonObject &object)
{
    AppPaths::ensureDir(QFileInfo(path).absolutePath());
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly))
        return false;
    file.write(QJsonDocument(object).toJson(QJsonDocument::Indented));
    return file.commit();
}

} // namespace

QString toString(ServiceActivity activity)
{
    switch (activity) {
    case ServiceActivity::Stopped: return QStringLiteral("Stopped");
    case ServiceActivity::Idle: return QStringLiteral("Running");
    case ServiceActivity::Busy: return QStringLiteral("Running a job");
    case ServiceActivity::Paused: return QStringLiteral("Paused");
    case ServiceActivity::Stopping: return QStringLiteral("Stopping");
    }
    return {};
}

QJsonObject JobRunState::toJson() const
{
    return {{"anchor", dateToString(anchor)},   {"fingerprint", fingerprint},
            {"lastStart", dateToString(lastStart)}, {"lastRun", dateToString(lastRun)},
            {"lastStatus", lastStatus},           {"lastReport", lastReport},
            {"lastExitCode", lastExitCode}};
}

JobRunState JobRunState::fromJson(const QJsonObject &o)
{
    JobRunState s;
    s.anchor = dateFromJson(o.value("anchor"));
    s.fingerprint = o.value("fingerprint").toString();
    s.lastStart = dateFromJson(o.value("lastStart"));
    s.lastRun = dateFromJson(o.value("lastRun"));
    s.lastStatus = o.value("lastStatus").toString();
    s.lastReport = o.value("lastReport").toString();
    s.lastExitCode = o.value("lastExitCode").toInt(-1);
    return s;
}

ServiceRunState ServiceRunState::load(const QString &path)
{
    ServiceRunState state;
    const auto o = readObject(path);
    if (!o)
        return state;
    const QJsonObject jobs = o->value("jobs").toObject();
    for (auto it = jobs.begin(); it != jobs.end(); ++it)
        state.jobs.insert(it.key(), JobRunState::fromJson(it.value().toObject()));
    return state;
}

bool ServiceRunState::save(const QString &path) const
{
    QJsonObject list;
    for (auto it = jobs.begin(); it != jobs.end(); ++it)
        list.insert(it.key(), it.value().toJson());
    return writeObject(path, {{"jobs", list}});
}

bool ServiceStatus::isFresh(const QDateTime &now) const
{
    return heartbeat.isValid() && heartbeat.secsTo(now) < ServiceInfo::kStaleSeconds;
}

QJsonObject ServiceStatus::toJson() const
{
    QJsonArray next;
    for (const NextRun &run : nextRuns)
        next.append(QJsonObject{{"job", run.job}, {"at", dateToString(run.at)}});
    return {{"activity", kActivityKeys[int(activity)]},
            {"pid", pid},
            {"version", version},
            {"binary", binary},
            {"started", dateToString(started)},
            {"heartbeat", dateToString(heartbeat)},
            {"currentJob", currentJob},
            {"currentSince", dateToString(currentSince)},
            {"nextRuns", next},
            {"lastError", lastError}};
}

ServiceStatus ServiceStatus::fromJson(const QJsonObject &o)
{
    ServiceStatus s;
    const QString activity = o.value("activity").toString();
    for (int i = 0; i < 5; ++i) {
        if (activity == QLatin1String(kActivityKeys[i]))
            s.activity = ServiceActivity(i);
    }
    s.pid = o.value("pid").toInteger();
    s.version = o.value("version").toString();
    s.binary = o.value("binary").toString();
    s.started = dateFromJson(o.value("started"));
    s.heartbeat = dateFromJson(o.value("heartbeat"));
    s.currentJob = o.value("currentJob").toString();
    s.currentSince = dateFromJson(o.value("currentSince"));
    for (const QJsonValue &v : o.value("nextRuns").toArray()) {
        const QJsonObject run = v.toObject();
        s.nextRuns.append({run.value("job").toString(), dateFromJson(run.value("at"))});
    }
    s.lastError = o.value("lastError").toString();
    return s;
}

ServiceStatus ServiceStatus::load(const QString &path, bool *ok)
{
    const auto o = readObject(path);
    if (ok)
        *ok = o.has_value();
    return o ? fromJson(*o) : ServiceStatus();
}

bool ServiceStatus::save(const QString &path) const
{
    return writeObject(path, toJson());
}

} // namespace obor
