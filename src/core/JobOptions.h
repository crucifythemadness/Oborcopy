#pragma once

#include "FsInfo.h"

#include <QString>
#include <QStringList>

namespace obor {

// Update = copy new and changed files, never delete (robocopy /E).
// Mirror = Update + delete destination items missing in the source (robocopy /MIR, rsync --delete).
enum class SyncMode { Update, Mirror };
enum class SymlinkMode { Skip, Follow };
enum class DeleteMode { Permanent, Archive };
enum class LogLevel { Summary, Changes, AllFiles };
enum class ToleranceMode { Auto, Custom };

struct FilterOptions
{
    QStringList includeFiles; // empty = all files
    QStringList excludeFiles; // robocopy /XF
    QStringList excludeDirs;  // robocopy /XD
    qint64 minSize = 0;       // bytes, 0 = no limit (robocopy /MIN)
    qint64 maxSize = 0;       // bytes, 0 = no limit (robocopy /MAX)
    int minAgeDays = 0;       // skip files newer than n days (robocopy /MINAGE)
    int maxAgeDays = 0;       // skip files older than n days (robocopy /MAXAGE)
    bool skipHidden = false;
    bool skipSystem = false;
    SymlinkMode symlinks = SymlinkMode::Skip; // robocopy /XJ
};

struct CopyOptions
{
    bool recursive = true;
    bool copyEmptyDirs = true;  // /E instead of /S
    int maxDepth = 0;           // 0 = unlimited (robocopy /LEV)
    bool moveFiles = false;     // delete source after copy (robocopy /MOV)
    bool skipOlder = false;     // robocopy /XO
    bool skipNewer = false;     // robocopy /XN
    bool overwriteAll = false;  // robocopy /IS /IT
    bool compareChecksum = false;
    bool verifyAfterCopy = false;
    int parallelCopies = 1;     // robocopy /MT
    int throughputLimitMBps = 0; // 0 = unlimited
    bool dryRun = false;        // robocopy /L
};

struct MetadataOptions
{
    bool fileTimes = true;   // robocopy /COPY:T
    bool dirTimes = true;    // robocopy /DCOPY:T
    bool attributes = true;  // Windows file attributes
    bool permissions = true; // POSIX permissions (Linux)
};

struct CompareOptions
{
    ToleranceMode toleranceMode = ToleranceMode::Auto; // Auto = 2 s on FAT/exFAT, else exact
    int toleranceMs = 2000;                            // used when Custom (robocopy /FFT)
    bool dstTolerance = false;                         // accept exactly 1 h offsets (robocopy /DST)
};

struct RetryOptions
{
    int retries = 3;        // robocopy /R
    int baseWaitMs = 2000;  // robocopy /W, doubled per attempt
    int maxWaitMs = 60000;
};

struct SafetyOptions
{
    DeleteMode deleteMode = DeleteMode::Permanent;
    bool thresholdEnabled = true;
    int thresholdPercent = 20; // ask/abort when more destination files would be deleted
    bool refuseEmptySource = true;
};

struct LogOptions
{
    QString logDir; // empty = AppPaths::defaultLogDir()
    LogLevel level = LogLevel::Changes;
    int keepLogs = 50;
};

struct JobOptions
{
    static constexpr int kSchemaVersion = 1;

    QString name;
    QString source;
    QString destination;
    VolumeRef sourceVolume;      // disk the folder was last seen on (VolumeCheck)
    VolumeRef destinationVolume;
    SyncMode mode = SyncMode::Update;
    FilterOptions filter;
    CopyOptions copy;
    MetadataOptions metadata;
    CompareOptions compare;
    RetryOptions retry;
    SafetyOptions safety;
    LogOptions log;

    // A new job with sensible defaults (excludes OS housekeeping folders).
    static JobOptions defaults(const QString &name);
    QString effectiveLogDir() const;
};

QString toString(SyncMode mode);

} // namespace obor
