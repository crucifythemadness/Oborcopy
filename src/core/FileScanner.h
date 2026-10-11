#pragma once

#include "FileEntry.h"
#include "JobOptions.h"
#include "PathFilter.h"

#include <QSet>

#include <functional>

class QFileInfo;

namespace obor {

class SyncControl;

struct ScanOptions
{
    bool recursive = true;
    int maxDepth = 0; // 0 = unlimited; 1 = root folder only
    SymlinkMode symlinks = SymlinkMode::Skip;
    bool caseInsensitive = false;
};

// Walks a directory tree and collects the files and folders that pass the filter.
class FileScanner
{
public:
    using ProgressFn = std::function<void(int entriesScanned)>;

    FileScanner(const PathFilter &filter, const ScanOptions &options, SyncControl *control);

    ScanResult scan(const QString &root, const ProgressFn &progress = {});

private:
    void scanDir(const QString &absDir, const QString &relDir, int depth);
    void handleEntry(const QFileInfo &info, const QString &relPath, int depth);
    void addDir(const QFileInfo &info, const QString &relPath, int depth);
    void addFile(const QFileInfo &info, const QString &relPath);
    bool isLink(const QFileInfo &info) const;
    void reportProgress();

    const PathFilter &m_filter;
    ScanOptions m_options;
    SyncControl *m_control;
    ProgressFn m_progress;
    ScanResult m_result;
    QSet<QString> m_visited; // canonical paths, guards against symlink loops
    int m_count = 0;
};

} // namespace obor
