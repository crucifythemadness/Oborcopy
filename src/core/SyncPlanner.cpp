#include "SyncPlanner.h"

#include <algorithm>

namespace obor {

namespace {

SyncAction makeAction(ActionType type, const FileEntry &entry, const QString &reason)
{
    SyncAction action;
    action.type = type;
    action.relPath = entry.relPath;
    action.size = entry.size;
    action.mtimeMs = entry.mtimeMs;
    action.permissions = entry.permissions;
    action.reason = reason;
    return action;
}

int depthOf(const QString &relPath)
{
    return int(relPath.count(QLatin1Char('/')));
}

} // namespace

SyncPlanner::SyncPlanner(const JobOptions &job, const FileComparator &comparator,
                         ContentDiffersFn contentDiffers)
    : m_job(job)
    , m_comparator(comparator)
    , m_contentDiffers(std::move(contentDiffers))
{
}

SyncPlan SyncPlanner::build(const ScanResult &source, const ScanResult &dest) const
{
    SyncPlan plan;
    plan.staleTemps = dest.staleTemps;
    plan.scanErrors = source.errors + dest.errors;
    plan.destFileCount = int(dest.files.size());
    planDirs(source, dest, plan);
    planFiles(source, dest, plan);
    if (m_job.mode == SyncMode::Mirror)
        planDeletions(source, dest, plan);
    sortPlan(plan);
    evaluateThreshold(plan);
    return plan;
}

void SyncPlanner::planDirs(const ScanResult &source, const ScanResult &dest, SyncPlan &plan) const
{
    for (auto it = source.dirs.cbegin(); it != source.dirs.cend(); ++it) {
        if (m_job.copy.copyEmptyDirs && !dest.dirs.contains(it.key()))
            plan.createDirs << makeAction(ActionType::CreateDir, it.value(), QStringLiteral("new folder"));
        if (m_job.metadata.dirTimes)
            plan.dirTimes << makeAction(ActionType::Skip, it.value(), QString());
    }
}

void SyncPlanner::planFiles(const ScanResult &source, const ScanResult &dest, SyncPlan &plan) const
{
    for (auto it = source.files.cbegin(); it != source.files.cend(); ++it) {
        ++plan.filesChecked;
        const auto found = dest.files.constFind(it.key());
        planFile(it.value(), found == dest.files.cend() ? nullptr : &found.value(), plan);
    }
}

void SyncPlanner::planFile(const FileEntry &src, const FileEntry *dst, SyncPlan &plan) const
{
    QString reason;
    ActionType type = ActionType::Skip;
    if (!dst) {
        type = ActionType::CopyNew;
        reason = QStringLiteral("new file");
    } else if (m_job.copy.overwriteAll) {
        type = ActionType::Update;
        reason = QStringLiteral("always overwrite");
    } else {
        reason = updateReason(src, *dst);
        if (!reason.isEmpty())
            type = ActionType::Update;
    }
    if (type == ActionType::Skip) {
        ++plan.unchanged;
        if (m_job.log.level == LogLevel::AllFiles)
            plan.skipped << makeAction(ActionType::Skip, src, QStringLiteral("unchanged"));
        return;
    }
    plan.copies << makeAction(type, src, reason);
    plan.bytesToCopy += src.size;
}

// Empty result = the destination file is up to date (or must be kept by an option).
QString SyncPlanner::updateReason(const FileEntry &src, const FileEntry &dst) const
{
    const Comparison cmp = m_comparator.compare(src, dst);
    if (cmp.identical()) {
        if (m_job.copy.compareChecksum && m_contentDiffers && m_contentDiffers(src.relPath))
            return QStringLiteral("content differs");
        return {};
    }
    if (cmp.timeOrder < 0 && m_job.copy.skipOlder)
        return {};
    if (cmp.timeOrder > 0 && m_job.copy.skipNewer)
        return {};
    if (cmp.timeOrder > 0)
        return QStringLiteral("source newer");
    if (cmp.timeOrder < 0)
        return QStringLiteral("source older");
    return QStringLiteral("size changed");
}

void SyncPlanner::planDeletions(const ScanResult &source, const ScanResult &dest,
                                SyncPlan &plan) const
{
    const QString reason = QStringLiteral("not in source");
    for (auto it = dest.files.cbegin(); it != dest.files.cend(); ++it) {
        if (!source.files.contains(it.key()))
            plan.deletions << makeAction(ActionType::DeleteFile, it.value(), reason);
    }
    for (auto it = dest.dirs.cbegin(); it != dest.dirs.cend(); ++it) {
        if (!source.dirs.contains(it.key()))
            plan.deletions << makeAction(ActionType::DeleteDir, it.value(), reason);
    }
}

void SyncPlanner::evaluateThreshold(SyncPlan &plan) const
{
    const int fileDeletions = plan.count(ActionType::DeleteFile);
    if (plan.destFileCount > 0)
        plan.deletionPercent = 100.0 * fileDeletions / plan.destFileCount;
    plan.thresholdExceeded = m_job.safety.thresholdEnabled && fileDeletions > 0
                             && plan.deletionPercent > m_job.safety.thresholdPercent;
}

void SyncPlanner::sortPlan(SyncPlan &plan)
{
    auto byPath = [](const SyncAction &a, const SyncAction &b) { return a.relPath < b.relPath; };
    std::sort(plan.copies.begin(), plan.copies.end(), byPath);
    std::sort(plan.skipped.begin(), plan.skipped.end(), byPath);
    std::sort(plan.createDirs.begin(), plan.createDirs.end(), [](const auto &a, const auto &b) {
        return std::make_pair(depthOf(a.relPath), a.relPath) < std::make_pair(depthOf(b.relPath), b.relPath);
    });
    // Files before folders; folders deepest first so each is empty when removed.
    std::sort(plan.deletions.begin(), plan.deletions.end(), [](const auto &a, const auto &b) {
        const bool aDir = a.type == ActionType::DeleteDir;
        const bool bDir = b.type == ActionType::DeleteDir;
        if (aDir != bDir)
            return !aDir;
        if (aDir && depthOf(a.relPath) != depthOf(b.relPath))
            return depthOf(a.relPath) > depthOf(b.relPath);
        return a.relPath < b.relPath;
    });
}

} // namespace obor
