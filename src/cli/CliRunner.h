#pragma once

#include "JobOptions.h"
#include "Progress.h"
#include "RunReport.h"
#include "SyncPlan.h"

#include <QTextStream>

#include <optional>

namespace obor {

// Headless front end, e.g. for Windows Task Scheduler or cron:
//   oborcopy-cli --job "Music Backup" --yes
// Exit code (bit mask, like robocopy): 0 nothing to do, 1 files copied,
// 2 extra files deleted, 8 some items failed, 16 fatal error / aborted.
class CliRunner
{
public:
    enum ExitBits { NothingToDo = 0, Copied = 1, Deleted = 2, Errors = 8, Fatal = 16 };

    CliRunner();
    int run(const QStringList &arguments);

    static int exitCode(const RunReport &report);

private:
    struct Options
    {
        QString jobName;
        QString jobFile;
        bool listJobs = false;
        bool dryRun = false;
        bool yes = false;
        bool force = false;
        bool quiet = false;
        bool verbose = false;
        bool infoOnly = false; // --help or --version was handled
    };

    std::optional<Options> parse(const QStringList &arguments);
    int listJobs();
    std::optional<JobOptions> loadJob(const Options &options);
    int runJob(JobOptions job, const Options &options);
    bool confirmPlan(const SyncPlan &plan, const Options &options);
    void printProgress(const ProgressInfo &info);
    void printSummary(const RunReport &report);

    QTextStream m_out;
    QTextStream m_err;
    SyncPhase m_lastPhase = SyncPhase::Idle;
};

} // namespace obor
