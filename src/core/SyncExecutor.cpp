#include "SyncExecutor.h"

#include "FileComparator.h"
#include "Platform.h"
#include "Progress.h"
#include "ReportSink.h"
#include "RunLogger.h"
#include "SyncControl.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>

#include <thread>
#include <vector>

namespace obor {

SyncExecutor::SyncExecutor(const JobOptions &job, const FileComparator &comparator,
                           SyncControl &control, ProgressTracker &tracker, ReportSink &sink,
                           RunLogger *logger)
    : m_job(job)
    , m_comparator(comparator)
    , m_control(control)
    , m_tracker(tracker)
    , m_sink(sink)
    , m_logger(logger)
    , m_retry(job.retry)
    , m_throttle(job.copy.throughputLimitMBps)
    , m_archiveStamp(QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd_HHmmss")))
{
    m_copySettings.fileTimes = job.metadata.fileTimes;
    m_copySettings.attributes = job.metadata.attributes;
    m_copySettings.permissions = job.metadata.permissions;
    m_copySettings.verify = job.copy.verifyAfterCopy;
}

void SyncExecutor::run(const SyncPlan &plan)
{
    removeStaleTemps(plan.staleTemps);
    if (!plan.deletions.isEmpty()) {
        m_tracker.setPhase(SyncPhase::Deleting);
        runDeletions(plan.deletions);
    }
    m_tracker.setTotals(int(plan.copies.size()), plan.bytesToCopy);
    m_tracker.setPhase(SyncPhase::Copying);
    createDirs(plan.createDirs);
    runCopies(plan.copies);
    m_tracker.setPhase(SyncPhase::Finalizing);
    applyDirTimes(plan.dirTimes);
}

void SyncExecutor::removeStaleTemps(const QStringList &relPaths)
{
    for (const QString &rel : relPaths) {
        Platform::clearProtectiveAttributes(destPath(rel));
        if (QFile::remove(destPath(rel)))
            info(QStringLiteral("Removed leftover temporary file %1").arg(rel));
    }
}

void SyncExecutor::runDeletions(const QVector<SyncAction> &actions)
{
    for (const SyncAction &action : actions) {
        if (!m_control.checkpoint())
            return;
        m_tracker.beginFile(action.relPath, 0);
        if (action.type == ActionType::DeleteDir)
            deleteDir(action);
        else
            deleteFile(action);
    }
}

void SyncExecutor::deleteFile(const SyncAction &action)
{
    const QString path = destPath(action.relPath);
    const bool archive = m_job.safety.deleteMode == DeleteMode::Archive;
    int attempts = 0;
    const OpResult result = withRetry(action.relPath, [&]() {
        Platform::clearProtectiveAttributes(path);
        QFile file(path);
        if (archive) {
            const QString target = archivePath(action.relPath);
            QDir().mkpath(QFileInfo(target).absolutePath());
            if (file.rename(target))
                return OpResult::success();
        } else if (file.remove()) {
            return OpResult::success();
        }
        return OpResult::failure(file.errorString());
    }, &attempts);
    if (result.cancelled)
        return;
    if (!result.ok) {
        addError(archive ? QStringLiteral("Archive") : QStringLiteral("Delete"), action.relPath,
                 result, attempts);
        return;
    }
    m_sink.add({ItemKind::Deleted, action.relPath, action.size, {},
                archive ? QStringLiteral("moved to archive") : action.reason, attempts});
}

void SyncExecutor::deleteDir(const SyncAction &action)
{
    const QString path = destPath(action.relPath);
    if (!QDir(path).isEmpty()) {
        info(QStringLiteral("Kept folder %1 (contains excluded or undeletable items)")
                 .arg(action.relPath));
        return;
    }
    int attempts = 0;
    const OpResult result = withRetry(action.relPath, [&]() {
        Platform::clearProtectiveAttributes(path);
        return QDir().rmdir(path) ? OpResult::success()
                                  : OpResult::failure(QStringLiteral("Cannot remove folder"));
    }, &attempts);
    if (result.cancelled)
        return;
    if (!result.ok) {
        addError(QStringLiteral("Delete folder"), action.relPath, result, attempts);
        return;
    }
    m_sink.add({ItemKind::DirDeleted, action.relPath, 0, {}, action.reason, attempts});
}

void SyncExecutor::createDirs(const QVector<SyncAction> &actions)
{
    for (const SyncAction &action : actions) {
        if (!m_control.checkpoint())
            return;
        const QString path = destPath(action.relPath);
        int attempts = 0;
        const OpResult result = withRetry(action.relPath, [&]() {
            return QDir().mkpath(path) ? OpResult::success()
                                       : OpResult::failure(QStringLiteral("Cannot create folder"));
        }, &attempts);
        if (result.cancelled)
            return;
        if (result.ok)
            m_sink.add({ItemKind::DirCreated, action.relPath, 0, {}, {}, attempts});
        else
            addError(QStringLiteral("Create folder"), action.relPath, result, attempts);
    }
}

void SyncExecutor::runCopies(const QVector<SyncAction> &actions)
{
    std::atomic<int> next{0};
    const int threads = qBound(1, m_job.copy.parallelCopies, 16);
    if (threads == 1 || actions.size() < 2) {
        copyWorker(actions, next);
        return;
    }
    std::vector<std::thread> workers;
    for (int i = 1; i < threads; ++i)
        workers.emplace_back([this, &actions, &next]() { copyWorker(actions, next); });
    copyWorker(actions, next);
    for (std::thread &worker : workers)
        worker.join();
}

void SyncExecutor::copyWorker(const QVector<SyncAction> &actions, std::atomic<int> &next)
{
    FileCopier copier(m_copySettings, m_control, &m_throttle,
                      [this](qint64 delta) { m_tracker.addBytes(delta); });
    while (m_control.checkpoint()) {
        const int index = next.fetch_add(1);
        if (index >= actions.size())
            return;
        copyOne(copier, actions.at(index));
    }
}

void SyncExecutor::copyOne(FileCopier &copier, const SyncAction &action)
{
    m_tracker.beginFile(action.relPath, action.size);
    const CopyRequest request{sourcePath(action.relPath), destPath(action.relPath),
                              action.mtimeMs, action.permissions};
    int attempts = 0;
    const OpResult result = withRetry(action.relPath, [&]() { return copier.copy(request); },
                                      &attempts);
    if (result.cancelled)
        return;
    m_tracker.fileDone();
    const bool isNew = action.type == ActionType::CopyNew;
    if (!result.ok) {
        addError(isNew ? QStringLiteral("Copy") : QStringLiteral("Update"), action.relPath,
                 result, attempts);
        return;
    }
    m_sink.add({isNew ? ItemKind::Copied : ItemKind::Updated, action.relPath, action.size, {},
                action.reason, attempts});
    if (m_job.copy.moveFiles)
        removeSource(action);
}

void SyncExecutor::removeSource(const SyncAction &action)
{
    const QString path = sourcePath(action.relPath);
    int attempts = 0;
    const OpResult result = withRetry(action.relPath, [&]() {
        Platform::clearProtectiveAttributes(path);
        QFile file(path);
        return file.remove() ? OpResult::success() : OpResult::failure(file.errorString());
    }, &attempts);
    if (!result.ok && !result.cancelled)
        addError(QStringLiteral("Remove source (move)"), action.relPath, result, attempts);
}

void SyncExecutor::applyDirTimes(const QVector<SyncAction> &actions)
{
    for (const SyncAction &action : actions) {
        if (m_control.isCancelled())
            return;
        const QString path = destPath(action.relPath);
        const QFileInfo dirInfo(path);
        if (!dirInfo.isDir()
            || m_comparator.timesEqual(action.mtimeMs, dirInfo.lastModified().toMSecsSinceEpoch()))
            continue;
        if (!Platform::setDirModificationTime(path, action.mtimeMs))
            info(QStringLiteral("Could not set folder time of %1").arg(action.relPath));
    }
}

OpResult SyncExecutor::withRetry(const QString &relPath, const std::function<OpResult()> &op,
                                 int *attempts)
{
    OpResult result;
    int attempt = 0;
    while (true) {
        ++attempt;
        result = op();
        if (result.ok || result.cancelled || !result.retryable || attempt > m_retry.maxRetries())
            break;
        const int waitMs = m_retry.delayMs(attempt);
        info(QStringLiteral("Retry %1/%2 in %3 s for %4: %5")
                 .arg(attempt)
                 .arg(m_retry.maxRetries())
                 .arg(waitMs / 1000.0, 0, 'f', 1)
                 .arg(relPath, result.error));
        if (!m_control.sleepFor(waitMs)) {
            result = OpResult::aborted();
            break;
        }
    }
    *attempts = attempt;
    return result;
}

void SyncExecutor::addError(const QString &operation, const QString &relPath,
                            const OpResult &result, int attempts)
{
    m_sink.add({ItemKind::Error, relPath, 0, operation, result.error, attempts});
}

void SyncExecutor::info(const QString &text)
{
    if (m_logger)
        m_logger->info(text);
}

QString SyncExecutor::sourcePath(const QString &relPath) const
{
    return m_job.source + QLatin1Char('/') + relPath;
}

QString SyncExecutor::destPath(const QString &relPath) const
{
    return m_job.destination + QLatin1Char('/') + relPath;
}

QString SyncExecutor::archivePath(const QString &relPath) const
{
    return QStringLiteral("%1/%2/%3/%4")
        .arg(m_job.destination, kArchiveDirName, m_archiveStamp, relPath);
}

} // namespace obor
