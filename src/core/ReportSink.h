#pragma once

#include "RunReport.h"

#include <QMutex>

namespace obor {

class ProgressTracker;
class RunLogger;

// Single entry point for results: records them in the report, writes the log
// line and updates the live counters. Safe to call from several threads.
class ReportSink
{
public:
    ReportSink(RunReport &report, RunLogger *logger, ProgressTracker *tracker);

    void add(const ReportItem &item);
    RunStats stats() const;

private:
    void log(const ReportItem &item);

    RunReport &m_report;
    RunLogger *m_logger;
    ProgressTracker *m_tracker;
    mutable QMutex m_mutex;
};

} // namespace obor
