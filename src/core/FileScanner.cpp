#include "FileScanner.h"

#include "SyncControl.h"

#include <QDir>
#include <QFileInfo>

namespace obor {

FileScanner::FileScanner(const PathFilter &filter, const ScanOptions &options,
                         SyncControl *control)
    : m_filter(filter)
    , m_options(options)
    , m_control(control)
{
}

ScanResult FileScanner::scan(const QString &root, const ProgressFn &progress)
{
    m_result = ScanResult();
    m_visited.clear();
    m_count = 0;
    m_progress = progress;

    const QFileInfo rootInfo(root);
    m_result.rootExists = rootInfo.isDir();
    if (m_result.rootExists) {
        m_visited.insert(rootInfo.canonicalFilePath());
        scanDir(rootInfo.absoluteFilePath(), QString(), 1);
    }
    if (m_progress)
        m_progress(m_count);
    return std::move(m_result);
}

void FileScanner::scanDir(const QString &absDir, const QString &relDir, int depth)
{
    if (m_result.cancelled)
        return;
    QDir dir(absDir);
    if (!dir.isReadable()) {
        m_result.errors.append({relDir, QStringLiteral("Folder is not readable")});
        return;
    }
    const QFileInfoList entries = dir.entryInfoList(
        QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden | QDir::System, QDir::Name);
    for (const QFileInfo &info : entries) {
        const QString rel = relDir.isEmpty() ? info.fileName()
                                             : relDir + QLatin1Char('/') + info.fileName();
        handleEntry(info, rel, depth);
        if (m_result.cancelled)
            return;
    }
}

void FileScanner::handleEntry(const QFileInfo &info, const QString &relPath, int depth)
{
    reportProgress();
    if (isLink(info) && m_options.symlinks == SymlinkMode::Skip)
        return;
    if (info.isDir()) {
        // The archive folder lives in the destination root and is never synced.
        if (depth == 1 && info.fileName() == kArchiveDirName)
            return;
        addDir(info, relPath, depth);
        return;
    }
    if (info.fileName().endsWith(kTempSuffix)) {
        m_result.staleTemps << relPath;
        return;
    }
    if (info.isFile())
        addFile(info, relPath);
}

void FileScanner::addDir(const QFileInfo &info, const QString &relPath, int depth)
{
    if (!m_options.recursive || (m_options.maxDepth > 0 && depth >= m_options.maxDepth))
        return;
    if (!m_filter.acceptDir(info, relPath))
        return;
    if (isLink(info)) {
        const QString canonical = info.canonicalFilePath();
        if (canonical.isEmpty() || m_visited.contains(canonical))
            return; // broken link or loop
        m_visited.insert(canonical);
    }
    FileEntry entry;
    entry.relPath = relPath;
    entry.isDir = true;
    entry.mtimeMs = info.lastModified().toMSecsSinceEpoch();
    m_result.dirs.insert(compareKey(relPath, m_options.caseInsensitive), entry);
    scanDir(info.absoluteFilePath(), relPath, depth + 1);
}

void FileScanner::addFile(const QFileInfo &info, const QString &relPath)
{
    if (!m_filter.acceptFile(info, relPath))
        return;
    FileEntry entry;
    entry.relPath = relPath;
    entry.size = info.size();
    entry.mtimeMs = info.lastModified().toMSecsSinceEpoch();
    entry.permissions = info.permissions();
    m_result.files.insert(compareKey(relPath, m_options.caseInsensitive), entry);
}

bool FileScanner::isLink(const QFileInfo &info) const
{
    return info.isSymbolicLink() || info.isJunction();
}

void FileScanner::reportProgress()
{
    ++m_count;
    if (m_count % 256 != 0)
        return;
    if (m_control && !m_control->checkpoint())
        m_result.cancelled = true;
    if (m_progress)
        m_progress(m_count);
}

} // namespace obor
