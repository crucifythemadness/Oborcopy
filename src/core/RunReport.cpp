#include "RunReport.h"

#include <QJsonArray>
#include <QLocale>

namespace obor {

namespace {

const char *const kKindKeys[] = {"copied", "updated", "deleted", "dirCreated",
                                 "dirDeleted", "skipped", "error"};
const char *const kStatusKeys[] = {"completed", "completedWithErrors", "cancelled", "failed"};

template <typename Enum, std::size_t N>
Enum fromKey(const QString &key, const char *const (&keys)[N], Enum fallback)
{
    for (std::size_t i = 0; i < N; ++i) {
        if (key == QLatin1String(keys[i]))
            return Enum(i);
    }
    return fallback;
}

QJsonObject itemToJson(const ReportItem &item)
{
    QJsonObject o{{"kind", kKindKeys[int(item.kind)]}, {"path", item.relPath}};
    if (item.size)
        o.insert("size", item.size);
    if (!item.operation.isEmpty())
        o.insert("operation", item.operation);
    if (!item.message.isEmpty())
        o.insert("message", item.message);
    if (item.attempts > 1)
        o.insert("attempts", item.attempts);
    return o;
}

ReportItem itemFromJson(const QJsonObject &o)
{
    ReportItem item;
    item.kind = fromKey(o.value("kind").toString(), kKindKeys, ItemKind::Error);
    item.relPath = o.value("path").toString();
    item.size = o.value("size").toInteger();
    item.operation = o.value("operation").toString();
    item.message = o.value("message").toString();
    item.attempts = o.value("attempts").toInt(1);
    return item;
}

QJsonObject statsToJson(const RunStats &s)
{
    return {{"filesChecked", s.filesChecked}, {"unchanged", s.unchanged},
            {"copied", s.copied},             {"updated", s.updated},
            {"deleted", s.deleted},           {"dirsCreated", s.dirsCreated},
            {"dirsDeleted", s.dirsDeleted},   {"errors", s.errors},
            {"bytesCopied", s.bytesCopied}};
}

RunStats statsFromJson(const QJsonObject &o)
{
    RunStats s;
    s.filesChecked = o.value("filesChecked").toInt();
    s.unchanged = o.value("unchanged").toInt();
    s.copied = o.value("copied").toInt();
    s.updated = o.value("updated").toInt();
    s.deleted = o.value("deleted").toInt();
    s.dirsCreated = o.value("dirsCreated").toInt();
    s.dirsDeleted = o.value("dirsDeleted").toInt();
    s.errors = o.value("errors").toInt();
    s.bytesCopied = o.value("bytesCopied").toInteger();
    return s;
}

} // namespace

QString toString(ItemKind kind)
{
    switch (kind) {
    case ItemKind::Copied: return QStringLiteral("Copied");
    case ItemKind::Updated: return QStringLiteral("Updated");
    case ItemKind::Deleted: return QStringLiteral("Deleted");
    case ItemKind::DirCreated: return QStringLiteral("Folder created");
    case ItemKind::DirDeleted: return QStringLiteral("Folder deleted");
    case ItemKind::Skipped: return QStringLiteral("Unchanged");
    case ItemKind::Error: return QStringLiteral("Error");
    }
    return {};
}

QString toString(RunStatus status)
{
    switch (status) {
    case RunStatus::Completed: return QStringLiteral("Completed");
    case RunStatus::CompletedWithErrors: return QStringLiteral("Completed with errors");
    case RunStatus::Cancelled: return QStringLiteral("Cancelled");
    case RunStatus::Failed: return QStringLiteral("Failed");
    }
    return {};
}

void RunReport::addItem(const ReportItem &item)
{
    items.append(item);
    switch (item.kind) {
    case ItemKind::Copied: ++stats.copied; stats.bytesCopied += item.size; break;
    case ItemKind::Updated: ++stats.updated; stats.bytesCopied += item.size; break;
    case ItemKind::Deleted: ++stats.deleted; break;
    case ItemKind::DirCreated: ++stats.dirsCreated; break;
    case ItemKind::DirDeleted: ++stats.dirsDeleted; break;
    case ItemKind::Skipped: break;
    case ItemKind::Error: ++stats.errors; break;
    }
}

QString RunReport::summaryLine() const
{
    const QLocale locale;
    auto n = [&locale](int value) { return locale.toString(value); };
    if (dryRun) {
        return QStringLiteral("Preview: %1 files checked, %2 to copy, %3 to update, %4 to delete")
            .arg(n(stats.filesChecked), n(stats.copied), n(stats.updated), n(stats.deleted));
    }
    return QStringLiteral("%1 files checked, %2 copied, %3 updated, %4 deleted, %5 errors")
        .arg(n(stats.filesChecked), n(stats.copied), n(stats.updated), n(stats.deleted),
             n(stats.errors));
}

qint64 RunReport::durationMs() const
{
    return (started.isValid() && finished.isValid()) ? started.msecsTo(finished) : 0;
}

QJsonObject RunReport::toJson() const
{
    QJsonArray list;
    for (const ReportItem &item : items)
        list.append(itemToJson(item));
    return {{"jobName", jobName},
            {"source", source},
            {"destination", destination},
            {"mode", toString(mode)},
            {"dryRun", dryRun},
            {"trigger", scheduled ? "scheduled" : "manual"},
            {"started", started.toString(Qt::ISODateWithMs)},
            {"finished", finished.toString(Qt::ISODateWithMs)},
            {"status", kStatusKeys[int(status)]},
            {"fatalError", fatalError},
            {"logFile", logFile},
            {"stats", statsToJson(stats)},
            {"items", list}};
}

RunReport RunReport::fromJson(const QJsonObject &o)
{
    RunReport r;
    r.jobName = o.value("jobName").toString();
    r.source = o.value("source").toString();
    r.destination = o.value("destination").toString();
    r.mode = o.value("mode").toString() == QLatin1String("Mirror") ? SyncMode::Mirror
                                                                  : SyncMode::Update;
    r.dryRun = o.value("dryRun").toBool();
    r.scheduled = o.value("trigger").toString() == QLatin1String("scheduled");
    r.started = QDateTime::fromString(o.value("started").toString(), Qt::ISODateWithMs);
    r.finished = QDateTime::fromString(o.value("finished").toString(), Qt::ISODateWithMs);
    r.status = fromKey(o.value("status").toString(), kStatusKeys, RunStatus::Failed);
    r.fatalError = o.value("fatalError").toString();
    r.logFile = o.value("logFile").toString();
    r.stats = statsFromJson(o.value("stats").toObject());
    for (const QJsonValue &v : o.value("items").toArray())
        r.items.append(itemFromJson(v.toObject()));
    return r;
}

} // namespace obor
