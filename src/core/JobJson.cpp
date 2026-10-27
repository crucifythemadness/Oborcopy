#include "JobJson.h"

#include <QJsonArray>

#include <utility>

namespace obor::JobJson {

namespace {

template <typename Enum, std::size_t N>
QString enumToString(Enum value, const std::pair<Enum, const char *> (&table)[N])
{
    for (const auto &entry : table) {
        if (entry.first == value)
            return QString::fromLatin1(entry.second);
    }
    return QString::fromLatin1(table[0].second);
}

template <typename Enum, std::size_t N>
Enum enumFromString(const QJsonValue &value, Enum fallback,
                    const std::pair<Enum, const char *> (&table)[N])
{
    const QString text = value.toString();
    for (const auto &entry : table) {
        if (text.compare(QLatin1String(entry.second), Qt::CaseInsensitive) == 0)
            return entry.first;
    }
    return fallback;
}

const std::pair<SyncMode, const char *> kModes[] = {{SyncMode::Update, "update"},
                                                     {SyncMode::Mirror, "mirror"}};
const std::pair<SymlinkMode, const char *> kSymlinks[] = {{SymlinkMode::Skip, "skip"},
                                                           {SymlinkMode::Follow, "follow"}};
const std::pair<DeleteMode, const char *> kDeleteModes[] = {{DeleteMode::Permanent, "permanent"},
                                                             {DeleteMode::Archive, "archive"}};
const std::pair<LogLevel, const char *> kLogLevels[] = {{LogLevel::Summary, "summary"},
                                                         {LogLevel::Changes, "changes"},
                                                         {LogLevel::AllFiles, "all"}};
const std::pair<ToleranceMode, const char *> kTolerances[] = {{ToleranceMode::Auto, "auto"},
                                                               {ToleranceMode::Custom, "custom"}};

QJsonArray toArray(const QStringList &list)
{
    return QJsonArray::fromStringList(list);
}

QStringList toStringList(const QJsonValue &value, const QStringList &fallback)
{
    if (!value.isArray())
        return fallback;
    QStringList result;
    for (const QJsonValue &item : value.toArray())
        result << item.toString();
    return result;
}

QJsonObject filterToJson(const FilterOptions &f)
{
    return {{"includeFiles", toArray(f.includeFiles)},
            {"excludeFiles", toArray(f.excludeFiles)},
            {"excludeDirs", toArray(f.excludeDirs)},
            {"minSize", f.minSize},
            {"maxSize", f.maxSize},
            {"minAgeDays", f.minAgeDays},
            {"maxAgeDays", f.maxAgeDays},
            {"skipHidden", f.skipHidden},
            {"skipSystem", f.skipSystem},
            {"symlinks", enumToString(f.symlinks, kSymlinks)}};
}

FilterOptions filterFromJson(const QJsonObject &o, const FilterOptions &d)
{
    FilterOptions f;
    f.includeFiles = toStringList(o.value("includeFiles"), d.includeFiles);
    f.excludeFiles = toStringList(o.value("excludeFiles"), d.excludeFiles);
    f.excludeDirs = toStringList(o.value("excludeDirs"), d.excludeDirs);
    f.minSize = o.value("minSize").toInteger(d.minSize);
    f.maxSize = o.value("maxSize").toInteger(d.maxSize);
    f.minAgeDays = o.value("minAgeDays").toInt(d.minAgeDays);
    f.maxAgeDays = o.value("maxAgeDays").toInt(d.maxAgeDays);
    f.skipHidden = o.value("skipHidden").toBool(d.skipHidden);
    f.skipSystem = o.value("skipSystem").toBool(d.skipSystem);
    f.symlinks = enumFromString(o.value("symlinks"), d.symlinks, kSymlinks);
    return f;
}

QJsonObject copyToJson(const CopyOptions &c)
{
    return {{"recursive", c.recursive},
            {"copyEmptyDirs", c.copyEmptyDirs},
            {"maxDepth", c.maxDepth},
            {"moveFiles", c.moveFiles},
            {"skipOlder", c.skipOlder},
            {"skipNewer", c.skipNewer},
            {"overwriteAll", c.overwriteAll},
            {"compareChecksum", c.compareChecksum},
            {"verifyAfterCopy", c.verifyAfterCopy},
            {"parallelCopies", c.parallelCopies},
            {"throughputLimitMBps", c.throughputLimitMBps},
            {"dryRun", c.dryRun}};
}

CopyOptions copyFromJson(const QJsonObject &o, const CopyOptions &d)
{
    CopyOptions c;
    c.recursive = o.value("recursive").toBool(d.recursive);
    c.copyEmptyDirs = o.value("copyEmptyDirs").toBool(d.copyEmptyDirs);
    c.maxDepth = o.value("maxDepth").toInt(d.maxDepth);
    c.moveFiles = o.value("moveFiles").toBool(d.moveFiles);
    c.skipOlder = o.value("skipOlder").toBool(d.skipOlder);
    c.skipNewer = o.value("skipNewer").toBool(d.skipNewer);
    c.overwriteAll = o.value("overwriteAll").toBool(d.overwriteAll);
    c.compareChecksum = o.value("compareChecksum").toBool(d.compareChecksum);
    c.verifyAfterCopy = o.value("verifyAfterCopy").toBool(d.verifyAfterCopy);
    c.parallelCopies = qBound(1, o.value("parallelCopies").toInt(d.parallelCopies), 16);
    c.throughputLimitMBps = o.value("throughputLimitMBps").toInt(d.throughputLimitMBps);
    c.dryRun = o.value("dryRun").toBool(d.dryRun);
    return c;
}

QJsonObject metadataToJson(const MetadataOptions &m)
{
    return {{"fileTimes", m.fileTimes},
            {"dirTimes", m.dirTimes},
            {"attributes", m.attributes},
            {"permissions", m.permissions}};
}

MetadataOptions metadataFromJson(const QJsonObject &o, const MetadataOptions &d)
{
    MetadataOptions m;
    m.fileTimes = o.value("fileTimes").toBool(d.fileTimes);
    m.dirTimes = o.value("dirTimes").toBool(d.dirTimes);
    m.attributes = o.value("attributes").toBool(d.attributes);
    m.permissions = o.value("permissions").toBool(d.permissions);
    return m;
}

QJsonObject compareToJson(const CompareOptions &c)
{
    return {{"toleranceMode", enumToString(c.toleranceMode, kTolerances)},
            {"toleranceMs", c.toleranceMs},
            {"dstTolerance", c.dstTolerance}};
}

CompareOptions compareFromJson(const QJsonObject &o, const CompareOptions &d)
{
    CompareOptions c;
    c.toleranceMode = enumFromString(o.value("toleranceMode"), d.toleranceMode, kTolerances);
    c.toleranceMs = o.value("toleranceMs").toInt(d.toleranceMs);
    c.dstTolerance = o.value("dstTolerance").toBool(d.dstTolerance);
    return c;
}

QJsonObject retryToJson(const RetryOptions &r)
{
    return {{"retries", r.retries}, {"baseWaitMs", r.baseWaitMs}, {"maxWaitMs", r.maxWaitMs}};
}

RetryOptions retryFromJson(const QJsonObject &o, const RetryOptions &d)
{
    RetryOptions r;
    r.retries = o.value("retries").toInt(d.retries);
    r.baseWaitMs = o.value("baseWaitMs").toInt(d.baseWaitMs);
    r.maxWaitMs = o.value("maxWaitMs").toInt(d.maxWaitMs);
    return r;
}

QJsonObject safetyToJson(const SafetyOptions &s)
{
    return {{"deleteMode", enumToString(s.deleteMode, kDeleteModes)},
            {"thresholdEnabled", s.thresholdEnabled},
            {"thresholdPercent", s.thresholdPercent},
            {"refuseEmptySource", s.refuseEmptySource}};
}

SafetyOptions safetyFromJson(const QJsonObject &o, const SafetyOptions &d)
{
    SafetyOptions s;
    s.deleteMode = enumFromString(o.value("deleteMode"), d.deleteMode, kDeleteModes);
    s.thresholdEnabled = o.value("thresholdEnabled").toBool(d.thresholdEnabled);
    s.thresholdPercent = o.value("thresholdPercent").toInt(d.thresholdPercent);
    s.refuseEmptySource = o.value("refuseEmptySource").toBool(d.refuseEmptySource);
    return s;
}

QJsonObject logToJson(const LogOptions &l)
{
    return {{"logDir", l.logDir},
            {"level", enumToString(l.level, kLogLevels)},
            {"keepLogs", l.keepLogs}};
}

LogOptions logFromJson(const QJsonObject &o, const LogOptions &d)
{
    LogOptions l;
    l.logDir = o.value("logDir").toString(d.logDir);
    l.level = enumFromString(o.value("level"), d.level, kLogLevels);
    l.keepLogs = o.value("keepLogs").toInt(d.keepLogs);
    return l;
}

} // namespace

QJsonObject toJson(const JobOptions &job)
{
    return {{"schemaVersion", JobOptions::kSchemaVersion},
            {"name", job.name},
            {"source", job.source},
            {"destination", job.destination},
            {"sourceVolume", toJson(job.sourceVolume)},
            {"destinationVolume", toJson(job.destinationVolume)},
            {"mode", enumToString(job.mode, kModes)},
            {"filter", filterToJson(job.filter)},
            {"copy", copyToJson(job.copy)},
            {"metadata", metadataToJson(job.metadata)},
            {"compare", compareToJson(job.compare)},
            {"retry", retryToJson(job.retry)},
            {"safety", safetyToJson(job.safety)},
            {"log", logToJson(job.log)}};
}

JobOptions fromJson(const QJsonObject &o)
{
    JobOptions job = JobOptions::defaults(o.value("name").toString());
    job.source = o.value("source").toString();
    job.destination = o.value("destination").toString();
    job.sourceVolume = volumeRefFromJson(o.value("sourceVolume"));
    job.destinationVolume = volumeRefFromJson(o.value("destinationVolume"));
    job.mode = enumFromString(o.value("mode"), job.mode, kModes);
    job.filter = filterFromJson(o.value("filter").toObject(), job.filter);
    job.copy = copyFromJson(o.value("copy").toObject(), job.copy);
    job.metadata = metadataFromJson(o.value("metadata").toObject(), job.metadata);
    job.compare = compareFromJson(o.value("compare").toObject(), job.compare);
    job.retry = retryFromJson(o.value("retry").toObject(), job.retry);
    job.safety = safetyFromJson(o.value("safety").toObject(), job.safety);
    job.log = logFromJson(o.value("log").toObject(), job.log);
    return job;
}

QJsonObject toJson(const VolumeRef &ref)
{
    return {{"label", ref.label}, {"root", ref.root}};
}

VolumeRef volumeRefFromJson(const QJsonValue &value)
{
    const QJsonObject o = value.toObject();
    return {o.value("label").toString(), o.value("root").toString()};
}

} // namespace obor::JobJson
