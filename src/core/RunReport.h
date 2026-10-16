#pragma once

#include "JobOptions.h"

#include <QDateTime>
#include <QJsonObject>
#include <QString>
#include <QVector>

namespace obor {

enum class ItemKind { Copied, Updated, Deleted, DirCreated, DirDeleted, Skipped, Error };
enum class RunStatus { Completed, CompletedWithErrors, Cancelled, Failed };

QString toString(ItemKind kind);
QString toString(RunStatus status);

struct ReportItem
{
    ItemKind kind = ItemKind::Copied;
    QString relPath;
    qint64 size = 0;
    QString operation; // for errors: what was attempted ("Copy", "Delete", ...)
    QString message;   // reason or OS error text
    int attempts = 1;
};

struct RunStats
{
    int filesChecked = 0;
    int unchanged = 0;
    int copied = 0;
    int updated = 0;
    int deleted = 0;
    int dirsCreated = 0;
    int dirsDeleted = 0;
    int errors = 0;
    qint64 bytesCopied = 0;
};

// Everything that happened in one run. Serialised next to the log file so the
// detailed summary can be reopened later from the run history.
struct RunReport
{
    QString jobName;
    QString source;
    QString destination;
    SyncMode mode = SyncMode::Update;
    bool dryRun = false;
    bool scheduled = false; // started by the scheduler service, not by the user
    QDateTime started;
    QDateTime finished;
    RunStatus status = RunStatus::Completed;
    QString fatalError;
    QString logFile;
    RunStats stats;
    QVector<ReportItem> items;

    void addItem(const ReportItem &item);
    // e.g. "1,204 files checked, 37 copied, 12 updated, 7 deleted, 2 errors"
    QString summaryLine() const;
    qint64 durationMs() const;

    QJsonObject toJson() const;
    static RunReport fromJson(const QJsonObject &object);
};

} // namespace obor
