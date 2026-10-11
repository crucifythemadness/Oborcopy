#pragma once

#include "FileComparator.h"
#include "JobOptions.h"
#include "SyncPlan.h"

#include <functional>

namespace obor {

// Builds a SyncPlan from the source and destination scans. Does not touch the disk
// except through the optional content comparison callback (checksum mode).
class SyncPlanner
{
public:
    // Returns true when the file contents differ (used when compareChecksum is on).
    using ContentDiffersFn = std::function<bool(const QString &relPath)>;

    SyncPlanner(const JobOptions &job, const FileComparator &comparator,
                ContentDiffersFn contentDiffers = {});

    SyncPlan build(const ScanResult &source, const ScanResult &dest) const;

private:
    void planDirs(const ScanResult &source, const ScanResult &dest, SyncPlan &plan) const;
    void planFiles(const ScanResult &source, const ScanResult &dest, SyncPlan &plan) const;
    void planFile(const FileEntry &src, const FileEntry *dst, SyncPlan &plan) const;
    QString updateReason(const FileEntry &src, const FileEntry &dst) const;
    void planDeletions(const ScanResult &source, const ScanResult &dest, SyncPlan &plan) const;
    void evaluateThreshold(SyncPlan &plan) const;
    static void sortPlan(SyncPlan &plan);

    const JobOptions &m_job;
    const FileComparator &m_comparator;
    ContentDiffersFn m_contentDiffers;
};

} // namespace obor
