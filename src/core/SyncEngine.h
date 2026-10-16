#pragma once

#include "FsInfo.h"
#include "JobOptions.h"
#include "Progress.h"
#include "RunReport.h"
#include "SyncPlan.h"

#include <functional>
#include <memory>

namespace obor {

class FileComparator;
class ReportSink;
class RunLogger;
class SyncControl;
struct ScanResult;

struct SyncCallbacks
{
    std::function<void(const ProgressInfo &)> progress;
    std::function<void(const QString &line)> log;
    // Called with the finished plan before anything is changed. Return false to
    // abort. If unset, the run proceeds without asking.
    std::function<bool(const SyncPlan &)> confirm;
};

// Runs one job from start to end: validate -> scan -> plan -> confirm -> execute -> report.
// Blocking; call it from a worker thread. Pause/cancel through the SyncControl.
class SyncEngine
{
public:
    SyncEngine(const JobOptions &job, SyncControl &control, SyncCallbacks callbacks);
    ~SyncEngine();

    // Marks the run as started by the scheduler service (shown in the run history).
    void setScheduled(bool scheduled) { m_report.scheduled = scheduled; }
    RunReport run();

private:
    void setup();
    void logHeader();
    bool validate();
    bool scan(ScanResult &source, ScanResult &dest);
    bool checkEmptySource(const ScanResult &source, const ScanResult &dest);
    SyncPlan buildPlan(const ScanResult &source, const ScanResult &dest);
    bool contentDiffers(const QString &relPath);
    void recordScanErrors(const SyncPlan &plan);
    void recordPreview(const SyncPlan &plan);
    bool confirm(const SyncPlan &plan);
    void execute(const SyncPlan &plan);
    RunReport finish();
    void fail(const QString &message);

    JobOptions m_job;
    SyncControl &m_control;
    SyncCallbacks m_callbacks;
    RunReport m_report;
    VolumeInfo m_sourceVolume;
    VolumeInfo m_destVolume;
    bool m_caseInsensitive = false;
    bool m_failed = false;
    std::unique_ptr<ProgressTracker> m_tracker;
    std::unique_ptr<RunLogger> m_logger;
    std::unique_ptr<ReportSink> m_sink;
    std::unique_ptr<FileComparator> m_comparator;
};

} // namespace obor
