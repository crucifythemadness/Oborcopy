#pragma once

#include <QDateTime>
#include <QJsonObject>
#include <QList>
#include <QString>
#include <QTime>
#include <QVector>

namespace obor {

enum class Frequency { Hourly, Daily, Weekly, Monthly };

QString toString(Frequency frequency);

// When the scheduler service runs one job. All times are local wall-clock times.
struct ScheduleEntry
{
    QString job;
    bool enabled = true;
    Frequency frequency = Frequency::Daily;
    int intervalHours = 1;               // Hourly: every n hours (1-23), counted from midnight
    QTime time = QTime(23, 30);          // Hourly: only the minute is used
    QList<int> weekdays = {1, 2, 3, 4, 5, 6, 7}; // Weekly: 1 = Monday ... 7 = Sunday
    int dayOfMonth = 1;                  // Monthly: 1-31 (clamped to the month), 0 = last day
    bool catchUp = true;                 // run once after start-up when a run was missed
    bool allowDeletions = true;          // Mirror jobs may delete (the threshold still applies)

    // First scheduled time strictly after 'after' (invalid if none, e.g. no weekday ticked).
    QDateTime nextRun(const QDateTime &after) const;
    // e.g. "Daily at 23:30", "Weekly on Mon, Thu at 07:00", "Every 4 hours at :15"
    QString describe() const;
    // Changes whenever the timing changes, so the service can re-anchor the entry.
    QString fingerprint() const;

    QJsonObject toJson() const;
    static ScheduleEntry fromJson(const QJsonObject &object);

private:
    QList<QTime> timesOn(const QDate &date) const;
};

// The list of scheduled jobs, stored as service/schedule.json. Written by the
// GUI, read by the service.
struct ServiceSchedule
{
    static constexpr int kSchemaVersion = 1;

    QVector<ScheduleEntry> entries;

    int indexOf(const QString &job) const;
    bool contains(const QString &job) const { return indexOf(job) >= 0; }
    bool renameJob(const QString &oldName, const QString &newName);
    bool removeJob(const QString &job);

    static ServiceSchedule load(const QString &path, QString *error = nullptr);
    bool save(const QString &path, QString *error = nullptr) const;
};

} // namespace obor
