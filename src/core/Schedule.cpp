#include "Schedule.h"

#include "AppPaths.h"

#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QLocale>
#include <QSaveFile>

#include <algorithm>

namespace obor {

namespace {

constexpr int kSearchDays = 400; // more than a year, so every monthly/weekly rule finds a date

const char *const kFrequencyKeys[] = {"hourly", "daily", "weekly", "monthly"};

Frequency frequencyFromKey(const QString &key)
{
    for (int i = 0; i < 4; ++i) {
        if (key == QLatin1String(kFrequencyKeys[i]))
            return Frequency(i);
    }
    return Frequency::Daily;
}

QString shortDayName(int weekday)
{
    return QLocale(QLocale::English).dayName(weekday, QLocale::ShortFormat);
}

} // namespace

QString toString(Frequency frequency)
{
    switch (frequency) {
    case Frequency::Hourly: return QStringLiteral("Every few hours");
    case Frequency::Daily: return QStringLiteral("Daily");
    case Frequency::Weekly: return QStringLiteral("Weekly");
    case Frequency::Monthly: return QStringLiteral("Monthly");
    }
    return {};
}

QList<QTime> ScheduleEntry::timesOn(const QDate &date) const
{
    switch (frequency) {
    case Frequency::Hourly: {
        QList<QTime> times;
        const int step = std::clamp(intervalHours, 1, 23);
        for (int h = 0; h < 24; h += step)
            times << QTime(h, time.minute());
        return times;
    }
    case Frequency::Daily:
        return {time};
    case Frequency::Weekly:
        return weekdays.contains(date.dayOfWeek()) ? QList<QTime>{time} : QList<QTime>{};
    case Frequency::Monthly: {
        const int last = date.daysInMonth();
        const int day = dayOfMonth <= 0 ? last : std::min(dayOfMonth, last);
        return date.day() == day ? QList<QTime>{time} : QList<QTime>{};
    }
    }
    return {};
}

QDateTime ScheduleEntry::nextRun(const QDateTime &after) const
{
    const QDate first = after.date();
    for (int d = 0; d <= kSearchDays; ++d) {
        const QDate date = first.addDays(d);
        for (const QTime &t : timesOn(date)) {
            // A time inside a DST gap is moved forward by Qt, which is what we want.
            const QDateTime candidate(date, QTime(t.hour(), t.minute()));
            if (candidate > after)
                return candidate;
        }
    }
    return {};
}

QString ScheduleEntry::describe() const
{
    const QString at = time.toString(QStringLiteral("HH:mm"));
    switch (frequency) {
    case Frequency::Hourly:
        if (intervalHours <= 1)
            return QStringLiteral("Every hour at :%1").arg(time.minute(), 2, 10, QLatin1Char('0'));
        return QStringLiteral("Every %1 hours at :%2")
            .arg(intervalHours)
            .arg(time.minute(), 2, 10, QLatin1Char('0'));
    case Frequency::Daily:
        return QStringLiteral("Daily at %1").arg(at);
    case Frequency::Weekly: {
        if (weekdays.size() == 7)
            return QStringLiteral("Daily at %1").arg(at);
        QList<int> days = weekdays;
        std::sort(days.begin(), days.end());
        QStringList names;
        for (int day : std::as_const(days))
            names << shortDayName(day);
        return QStringLiteral("Weekly on %1 at %2").arg(names.join(QStringLiteral(", ")), at);
    }
    case Frequency::Monthly:
        if (dayOfMonth <= 0)
            return QStringLiteral("Monthly on the last day at %1").arg(at);
        return QStringLiteral("Monthly on day %1 at %2").arg(dayOfMonth).arg(at);
    }
    return {};
}

QString ScheduleEntry::fingerprint() const
{
    QJsonObject o = toJson();
    // "enabled" stays in: re-enabling an entry must not catch up runs from while it was off.
    o.remove(QStringLiteral("job"));
    o.remove(QStringLiteral("allowDeletions"));
    o.remove(QStringLiteral("retryEnabled"));
    o.remove(QStringLiteral("retryCount"));
    o.remove(QStringLiteral("retryIntervalMinutes"));
    return QString::fromUtf8(QJsonDocument(o).toJson(QJsonDocument::Compact));
}

QJsonObject ScheduleEntry::toJson() const
{
    QJsonArray days;
    for (int day : weekdays)
        days.append(day);
    return {{"job", job},
            {"enabled", enabled},
            {"frequency", kFrequencyKeys[int(frequency)]},
            {"intervalHours", intervalHours},
            {"time", time.toString(QStringLiteral("HH:mm"))},
            {"weekdays", days},
            {"dayOfMonth", dayOfMonth},
            {"catchUp", catchUp},
            {"allowDeletions", allowDeletions},
            {"retryEnabled", retryEnabled},
            {"retryCount", retryCount},
            {"retryIntervalMinutes", retryIntervalMinutes}};
}

ScheduleEntry ScheduleEntry::fromJson(const QJsonObject &o)
{
    ScheduleEntry e;
    e.job = o.value("job").toString();
    e.enabled = o.value("enabled").toBool(e.enabled);
    e.frequency = frequencyFromKey(o.value("frequency").toString());
    e.intervalHours = std::clamp(o.value("intervalHours").toInt(e.intervalHours), 1, 23);
    const QTime time = QTime::fromString(o.value("time").toString(), QStringLiteral("HH:mm"));
    if (time.isValid())
        e.time = time;
    if (o.contains("weekdays")) {
        e.weekdays.clear();
        for (const QJsonValue &v : o.value("weekdays").toArray()) {
            const int day = v.toInt();
            if (day >= 1 && day <= 7 && !e.weekdays.contains(day))
                e.weekdays << day;
        }
    }
    e.dayOfMonth = std::clamp(o.value("dayOfMonth").toInt(e.dayOfMonth), 0, 31);
    e.catchUp = o.value("catchUp").toBool(e.catchUp);
    e.allowDeletions = o.value("allowDeletions").toBool(e.allowDeletions);
    e.retryEnabled = o.value("retryEnabled").toBool(e.retryEnabled);
    e.retryCount = std::clamp(o.value("retryCount").toInt(e.retryCount), 1, 24);
    e.retryIntervalMinutes = std::clamp(o.value("retryIntervalMinutes").toInt(e.retryIntervalMinutes), 1, 240);
    return e;
}

int ServiceSchedule::indexOf(const QString &job) const
{
    for (int i = 0; i < entries.size(); ++i) {
        if (entries.at(i).job.compare(job, Qt::CaseInsensitive) == 0)
            return i;
    }
    return -1;
}

bool ServiceSchedule::renameJob(const QString &oldName, const QString &newName)
{
    const int i = indexOf(oldName);
    if (i < 0)
        return false;
    entries[i].job = newName;
    return true;
}

bool ServiceSchedule::removeJob(const QString &job)
{
    const int i = indexOf(job);
    if (i < 0)
        return false;
    entries.remove(i);
    return true;
}

ServiceSchedule ServiceSchedule::load(const QString &path, QString *error)
{
    ServiceSchedule schedule;
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        if (error && file.exists())
            *error = file.errorString();
        return schedule;
    }
    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (!doc.isObject()) {
        if (error)
            *error = QStringLiteral("Invalid schedule file %1: %2").arg(path, parseError.errorString());
        return schedule;
    }
    for (const QJsonValue &v : doc.object().value("entries").toArray()) {
        ScheduleEntry entry = ScheduleEntry::fromJson(v.toObject());
        if (!entry.job.isEmpty() && !schedule.contains(entry.job))
            schedule.entries.append(entry);
    }
    return schedule;
}

bool ServiceSchedule::save(const QString &path, QString *error) const
{
    AppPaths::ensureDir(QFileInfo(path).absolutePath());
    QJsonArray list;
    for (const ScheduleEntry &entry : entries)
        list.append(entry.toJson());
    const QJsonObject o{{"schemaVersion", kSchemaVersion}, {"entries", list}};
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        if (error)
            *error = file.errorString();
        return false;
    }
    file.write(QJsonDocument(o).toJson(QJsonDocument::Indented));
    if (!file.commit()) {
        if (error)
            *error = file.errorString();
        return false;
    }
    return true;
}

} // namespace obor
