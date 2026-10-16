#include "CliRunner.h"

#include "AppInfo.h"
#include "AppPaths.h"
#include "JobStore.h"
#include "SyncControl.h"
#include "SyncEngine.h"

#include <QCommandLineParser>
#include <QLocale>

#include <cstdio>
#include <iostream>
#include <string>
#include <thread>

namespace obor {

CliRunner::CliRunner()
    : m_out(stdout)
    , m_err(stderr)
{
}

int CliRunner::run(const QStringList &arguments)
{
    const auto options = parse(arguments);
    if (!options)
        return Fatal;
    if (options->infoOnly)
        return NothingToDo;
    if (options->listJobs)
        return listJobs();
    auto job = loadJob(*options);
    if (!job)
        return Fatal;
    return runJob(*job, *options);
}

std::optional<CliRunner::Options> CliRunner::parse(const QStringList &arguments)
{
    // Help and version are printed by us: Qt's built-in handling shows a message
    // box on Windows when no console window is attached (e.g. Task Scheduler).
    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("Oborcopy - incremental copy and mirror tool (command line)"));
    const QCommandLineOption help({"h", "help"}, "Show this help.");
    const QCommandLineOption version("version", "Show the version.");
    const QCommandLineOption job({"j", "job"}, "Run the saved job with this name.", "name");
    const QCommandLineOption file({"f", "job-file"}, "Run the job stored in this JSON file.", "path");
    const QCommandLineOption list({"l", "list-jobs"}, "List the saved jobs.");
    const QCommandLineOption dry({"n", "dry-run"}, "Only show what would be done.");
    const QCommandLineOption yes({"y", "yes"}, "Allow deletions in Mirror mode without asking.");
    const QCommandLineOption force("force", "Run even if the deletion threshold is exceeded.");
    const QCommandLineOption quiet({"q", "quiet"}, "Print only the summary and errors.");
    const QCommandLineOption verbose({"v", "verbose"}, "Print every log line.");
    QCommandLineOption scheduled("scheduled", "Mark the run as started by the scheduler service.");
    QCommandLineOption stdinControl("stdin-control", "Cancel the run when 'cancel' is read from stdin.");
    scheduled.setFlags(QCommandLineOption::HiddenFromHelp);
    stdinControl.setFlags(QCommandLineOption::HiddenFromHelp);
    parser.addOptions({job, file, list, dry, yes, force, quiet, verbose, scheduled, stdinControl,
                       help, version});
    if (!parser.parse(arguments)) {
        m_err << "Error: " << parser.errorText() << "\n";
        return std::nullopt;
    }
    if (parser.isSet(help) || parser.isSet(version)) {
        m_out << AppInfo::name() << " " << AppInfo::version() << " (build " << AppInfo::buildDate() << ")\n";
        if (parser.isSet(help))
            m_out << "\n" << parser.helpText();
        m_out.flush();
        Options info;
        info.infoOnly = true;
        return info;
    }

    Options o{parser.value(job), parser.value(file), parser.isSet(list), parser.isSet(dry),
              parser.isSet(yes), parser.isSet(force), parser.isSet(quiet), parser.isSet(verbose),
              parser.isSet(scheduled), parser.isSet(stdinControl)};
    if (!o.listJobs && o.jobName.isEmpty() && o.jobFile.isEmpty()) {
        m_err << "Error: use --job <name>, --job-file <path> or --list-jobs (see --help)\n";
        return std::nullopt;
    }
    return o;
}

int CliRunner::listJobs()
{
    const JobStore store(AppPaths::jobsDir());
    m_out << "Jobs in " << store.directory() << ":\n";
    for (const QString &name : store.jobNames())
        m_out << "  " << name << "\n";
    return NothingToDo;
}

std::optional<JobOptions> CliRunner::loadJob(const Options &options)
{
    QString error;
    std::optional<JobOptions> job;
    if (!options.jobFile.isEmpty())
        job = JobStore::loadFile(options.jobFile, &error);
    else
        job = JobStore(AppPaths::jobsDir()).load(options.jobName, &error);
    if (!job)
        m_err << "Error: " << error << "\n";
    return job;
}

int CliRunner::runJob(JobOptions job, const Options &options)
{
    if (options.dryRun)
        job.copy.dryRun = true;
    // Shared, because the stdin watcher thread may outlive this function.
    const auto control = std::make_shared<SyncControl>();
    SyncCallbacks callbacks;
    if (!options.quiet)
        callbacks.progress = [this](const ProgressInfo &info) { printProgress(info); };
    if (options.verbose)
        callbacks.log = [this](const QString &line) { m_out << "\r" << line << "\n"; m_out.flush(); };
    callbacks.confirm = [this, &options](const SyncPlan &plan) { return confirmPlan(plan, options); };

    if (options.stdinControl)
        watchStdin(control);

    SyncEngine engine(job, *control, callbacks);
    engine.setScheduled(options.scheduled);
    const RunReport report = engine.run();
    printSummary(report);
    return exitCode(report);
}

bool CliRunner::confirmPlan(const SyncPlan &plan, const Options &options)
{
    if (plan.thresholdExceeded && !options.force) {
        m_err << QStringLiteral("\nAborted: %1 % of the destination files would be deleted, which exceeds "
                                "the job's threshold. Check the source, then use --force.\n")
                     .arg(plan.deletionPercent, 0, 'f', 1);
        return false;
    }
    if (plan.hasDeletions() && !options.yes) {
        m_err << QStringLiteral("\nAborted: %1 item(s) would be deleted in the destination. "
                                "Run with --dry-run to review them and with --yes to allow it.\n")
                     .arg(plan.deletions.size());
        return false;
    }
    return true;
}

void CliRunner::watchStdin(std::shared_ptr<SyncControl> control)
{
    // The scheduler service stops a run this way: the run ends as "Cancelled"
    // with a complete log instead of being killed. The thread is detached because
    // a blocking read cannot be interrupted; it ends with the process.
    std::thread([control = std::move(control)]() {
        std::string line;
        while (std::getline(std::cin, line)) {
            if (QString::fromStdString(line).trimmed() == QLatin1String("cancel"))
                break;
        }
        control->cancel();
    }).detach();
}

void CliRunner::printProgress(const ProgressInfo &info)
{
    if (info.phase != m_lastPhase) {
        m_lastPhase = info.phase;
        m_out << "\n" << toString(info.phase) << "...\n";
    }
    const QLocale locale;
    if (info.phase == SyncPhase::Scanning) {
        m_out << "\r  " << locale.toString(info.scanned) << " entries scanned   ";
    } else if (info.phase == SyncPhase::Copying && info.bytesTotal > 0) {
        const int percent = int(100.0 * double(info.bytesDone) / double(info.bytesTotal));
        m_out << QStringLiteral("\r  %1 %  %2/%3 files  %4/s      ")
                     .arg(percent, 3)
                     .arg(info.filesDone)
                     .arg(info.filesTotal)
                     .arg(locale.formattedDataSize(qint64(info.bytesPerSecond)));
    }
    m_out.flush();
}

void CliRunner::printSummary(const RunReport &report)
{
    m_out << "\n\n" << report.summaryLine() << "\n";
    m_out << "Status: " << toString(report.status) << "\n";
    if (!report.fatalError.isEmpty())
        m_err << "Error: " << report.fatalError << "\n";
    for (const ReportItem &item : report.items) {
        if (item.kind == ItemKind::Error)
            m_err << "  " << item.operation << " failed: " << item.relPath << " - " << item.message << "\n";
    }
    if (!report.logFile.isEmpty())
        m_out << "Log: " << report.logFile << "\n";
    m_out.flush();
    m_err.flush();
}

int CliRunner::exitCode(const RunReport &report)
{
    if (report.status == RunStatus::Failed || report.status == RunStatus::Cancelled)
        return Fatal;
    int code = NothingToDo;
    if (report.stats.copied + report.stats.updated > 0)
        code |= Copied;
    if (report.stats.deleted + report.stats.dirsDeleted > 0)
        code |= Deleted;
    if (report.stats.errors > 0)
        code |= Errors;
    return code;
}

} // namespace obor
