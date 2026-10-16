#include "SyncEngine.h"

#include "AppInfo.h"
#include "FileComparator.h"
#include "FileScanner.h"
#include "Hash.h"
#include "JobValidator.h"
#include "PathFilter.h"
#include "ReportSink.h"
#include "RunLogger.h"
#include "SyncControl.h"
#include "SyncExecutor.h"
#include "SyncPlanner.h"

#include <QDir>
#include <QLocale>

namespace obor {

namespace {

QString cleanRoot(const QString &path)
{
    return QDir::cleanPath(QDir::fromNativeSeparators(path.trimmed()));
}

} // namespace

SyncEngine::SyncEngine(const JobOptions &job, SyncControl &control, SyncCallbacks callbacks)
    : m_job(job)
    , m_control(control)
    , m_callbacks(std::move(callbacks))
{
    m_job.source = cleanRoot(job.source);
    m_job.destination = cleanRoot(job.destination);
}

SyncEngine::~SyncEngine() = default;

RunReport SyncEngine::run()
{
    setup();
    if (!validate())
        return finish();
    ScanResult source;
    ScanResult dest;
    if (!scan(source, dest) || !checkEmptySource(source, dest))
        return finish();
    const SyncPlan plan = buildPlan(source, dest);
    if (m_control.isCancelled())
        return finish();
    recordScanErrors(plan);
    if (m_job.copy.dryRun) {
        recordPreview(plan);
        return finish();
    }
    if (confirm(plan))
        execute(plan);
    return finish();
}

void SyncEngine::setup()
{
    m_report.jobName = m_job.name;
    m_report.source = m_job.source;
    m_report.destination = m_job.destination;
    m_report.mode = m_job.mode;
    m_report.dryRun = m_job.copy.dryRun;
    m_report.started = QDateTime::currentDateTime();
    m_tracker = std::make_unique<ProgressTracker>(m_callbacks.progress, &m_control);
    m_logger = std::make_unique<RunLogger>(m_job.log, m_job.effectiveLogDir(), m_job.name);
    m_logger->setEcho(m_callbacks.log);
    QString error;
    if (!m_logger->open(&error) && m_callbacks.log)
        m_callbacks.log(QStringLiteral("Warning: %1").arg(error));
    m_sink = std::make_unique<ReportSink>(m_report, m_logger.get(), m_tracker.get());
    logHeader();
}

void SyncEngine::logHeader()
{
    m_logger->info(QStringLiteral("%1 %2 (build %3)")
                       .arg(AppInfo::name(), AppInfo::version(), AppInfo::buildDate()));
    m_logger->info(QStringLiteral("Job: %1   Mode: %2%3")
                       .arg(m_job.name, toString(m_job.mode),
                            m_job.copy.dryRun ? QStringLiteral(" (preview only)") : QString()));
    if (m_report.scheduled)
        m_logger->info(QStringLiteral("Started by the Oborcopy Scheduler service"));
    m_logger->info(QStringLiteral("Source:      %1").arg(m_job.source));
    m_logger->info(QStringLiteral("Destination: %1").arg(m_job.destination));
}

bool SyncEngine::validate()
{
    m_tracker->setPhase(SyncPhase::Validating);
    const QString error = JobValidator::validate(m_job);
    if (!error.isEmpty()) {
        fail(error);
        return false;
    }
    m_sourceVolume = FsInfo::query(m_job.source);
    m_destVolume = FsInfo::query(m_job.destination);
    m_caseInsensitive = FsInfo::caseInsensitive(m_sourceVolume, m_destVolume);
    const int tolerance = m_job.compare.toleranceMode == ToleranceMode::Auto
                              ? FsInfo::autoToleranceMs(m_sourceVolume, m_destVolume)
                              : m_job.compare.toleranceMs;
    m_comparator = std::make_unique<FileComparator>(tolerance, m_job.compare.dstTolerance);
    m_logger->info(QStringLiteral("File systems: %1 -> %2, time tolerance %3 ms%4")
                       .arg(m_sourceVolume.fileSystem, m_destVolume.fileSystem)
                       .arg(tolerance)
                       .arg(m_job.compare.dstTolerance ? QStringLiteral(" (+DST)") : QString()));
    return true;
}

bool SyncEngine::scan(ScanResult &source, ScanResult &dest)
{
    m_tracker->setPhase(SyncPhase::Scanning);
    const PathFilter filter(m_job.filter, m_caseInsensitive);
    const ScanOptions options{m_job.copy.recursive, m_job.copy.maxDepth, m_job.filter.symlinks,
                              m_caseInsensitive};
    FileScanner scanner(filter, options, &m_control);
    source = scanner.scan(m_job.source, [this](int n) { m_tracker->setScanned(n); });
    const int sourceCount = int(source.files.size() + source.dirs.size());
    dest = scanner.scan(m_job.destination,
                        [this, sourceCount](int n) { m_tracker->setScanned(sourceCount + n); });
    m_logger->info(QStringLiteral("Scanned: source %1 files / %2 folders, destination %3 files / %4 folders")
                       .arg(source.files.size()).arg(source.dirs.size())
                       .arg(dest.files.size()).arg(dest.dirs.size()));
    return !source.cancelled && !dest.cancelled && !m_control.isCancelled();
}

// Protects against mirroring an empty (e.g. unmounted) source onto a full backup.
bool SyncEngine::checkEmptySource(const ScanResult &source, const ScanResult &dest)
{
    if (m_job.mode != SyncMode::Mirror || !m_job.safety.refuseEmptySource)
        return true;
    if (!source.files.isEmpty() || !source.dirs.isEmpty() || dest.files.isEmpty())
        return true;
    fail(QStringLiteral("The source folder is empty but the destination is not. Mirroring would "
                        "delete everything in the destination, so the run was stopped."));
    return false;
}

SyncPlan SyncEngine::buildPlan(const ScanResult &source, const ScanResult &dest)
{
    m_tracker->setPhase(SyncPhase::Comparing);
    SyncPlanner::ContentDiffersFn differs;
    if (m_job.copy.compareChecksum)
        differs = [this](const QString &rel) { return contentDiffers(rel); };
    SyncPlanner planner(m_job, *m_comparator, differs);
    SyncPlan plan = planner.build(source, dest);
    plan.destBytesFree = m_destVolume.valid ? m_destVolume.bytesFree : -1;
    m_report.stats.filesChecked = plan.filesChecked;
    m_report.stats.unchanged = plan.unchanged;
    m_logger->info(QStringLiteral("Plan: %1 new, %2 updates, %3 deletions, %4 to copy")
                       .arg(plan.count(ActionType::CopyNew))
                       .arg(plan.count(ActionType::Update))
                       .arg(plan.deletions.size())
                       .arg(QLocale().formattedDataSize(plan.bytesToCopy)));
    for (const SyncAction &skip : plan.skipped)
        m_sink->add({ItemKind::Skipped, skip.relPath, skip.size, {}, skip.reason, 1});
    return plan;
}

bool SyncEngine::contentDiffers(const QString &relPath)
{
    m_tracker->beginFile(relPath, 0);
    const QByteArray a = Hash::fileSha256(m_job.source + QLatin1Char('/') + relPath, &m_control);
    const QByteArray b = Hash::fileSha256(m_job.destination + QLatin1Char('/') + relPath, &m_control);
    return a.isEmpty() || a != b;
}

void SyncEngine::recordScanErrors(const SyncPlan &plan)
{
    for (const ScanError &error : plan.scanErrors)
        m_sink->add({ItemKind::Error, error.relPath, 0, QStringLiteral("Scan"), error.message, 1});
}

void SyncEngine::recordPreview(const SyncPlan &plan)
{
    for (const SyncAction &a : plan.deletions) {
        const ItemKind kind = a.type == ActionType::DeleteDir ? ItemKind::DirDeleted : ItemKind::Deleted;
        m_sink->add({kind, a.relPath, a.size, {}, a.reason, 0});
    }
    for (const SyncAction &a : plan.createDirs)
        m_sink->add({ItemKind::DirCreated, a.relPath, 0, {}, a.reason, 0});
    for (const SyncAction &a : plan.copies) {
        const ItemKind kind = a.type == ActionType::CopyNew ? ItemKind::Copied : ItemKind::Updated;
        m_sink->add({kind, a.relPath, a.size, {}, a.reason, 0});
    }
}

bool SyncEngine::confirm(const SyncPlan &plan)
{
    if (!m_callbacks.confirm)
        return true;
    m_tracker->setPhase(SyncPhase::AwaitingConfirmation);
    if (m_callbacks.confirm(plan) && !m_control.isCancelled())
        return true;
    m_logger->info(QStringLiteral("Run aborted before any change was made."));
    m_control.cancel();
    return false;
}

void SyncEngine::execute(const SyncPlan &plan)
{
    if (!QDir().mkpath(m_job.destination)) {
        fail(QStringLiteral("Cannot create the destination folder %1").arg(m_job.destination));
        return;
    }
    SyncExecutor executor(m_job, *m_comparator, m_control, *m_tracker, *m_sink, m_logger.get());
    executor.run(plan);
}

RunReport SyncEngine::finish()
{
    m_report.finished = QDateTime::currentDateTime();
    if (m_failed)
        m_report.status = RunStatus::Failed;
    else if (m_control.isCancelled())
        m_report.status = RunStatus::Cancelled;
    else if (m_report.stats.errors > 0)
        m_report.status = RunStatus::CompletedWithErrors;
    else
        m_report.status = RunStatus::Completed;
    m_report.logFile = m_logger->filePath();
    m_logger->info(m_report.summaryLine());
    m_logger->info(QStringLiteral("Status: %1, duration %2 s")
                       .arg(toString(m_report.status))
                       .arg(m_report.durationMs() / 1000.0, 0, 'f', 1));
    m_logger->writeReport(m_report);
    m_logger->close();
    m_tracker->setPhase(SyncPhase::Finished);
    return m_report;
}

void SyncEngine::fail(const QString &message)
{
    m_failed = true;
    m_report.fatalError = message;
    m_logger->error(message);
}

} // namespace obor
