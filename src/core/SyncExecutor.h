#pragma once

#include "FileCopier.h"
#include "JobOptions.h"
#include "RetryPolicy.h"
#include "SyncPlan.h"
#include "Throttle.h"

#include <atomic>
#include <functional>

namespace obor {

class FileComparator;
class ProgressTracker;
class ReportSink;
class RunLogger;
class SyncControl;

// Carries out a SyncPlan: deletions, folder creation, copies (optionally in
// parallel), then folder timestamps. Every operation goes through the retry policy.
class SyncExecutor
{
public:
    SyncExecutor(const JobOptions &job, const FileComparator &comparator, SyncControl &control,
                 ProgressTracker &tracker, ReportSink &sink, RunLogger *logger);

    void run(const SyncPlan &plan);

private:
    void removeStaleTemps(const QStringList &relPaths);
    void runDeletions(const QVector<SyncAction> &actions);
    void deleteFile(const SyncAction &action);
    void deleteDir(const SyncAction &action);
    void createDirs(const QVector<SyncAction> &actions);
    void runCopies(const QVector<SyncAction> &actions);
    void copyWorker(const QVector<SyncAction> &actions, std::atomic<int> &next);
    void copyOne(FileCopier &copier, const SyncAction &action);
    void removeSource(const SyncAction &action);
    void applyDirTimes(const QVector<SyncAction> &actions);

    OpResult withRetry(const QString &relPath, const std::function<OpResult()> &op,
                       int *attempts);
    void addError(const QString &operation, const QString &relPath, const OpResult &result,
                  int attempts);
    void info(const QString &text);

    QString sourcePath(const QString &relPath) const;
    QString destPath(const QString &relPath) const;
    QString archivePath(const QString &relPath) const;

    const JobOptions &m_job;
    const FileComparator &m_comparator;
    SyncControl &m_control;
    ProgressTracker &m_tracker;
    ReportSink &m_sink;
    RunLogger *m_logger;
    RetryPolicy m_retry;
    Throttle m_throttle;
    CopySettings m_copySettings;
    QString m_archiveStamp;
};

} // namespace obor
