#include "ReportSink.h"

#include "Progress.h"
#include "RunLogger.h"

#include <QLocale>

namespace obor {

ReportSink::ReportSink(RunReport &report, RunLogger *logger, ProgressTracker *tracker)
    : m_report(report)
    , m_logger(logger)
    , m_tracker(tracker)
{
}

void ReportSink::add(const ReportItem &item)
{
    RunStats stats;
    {
        QMutexLocker lock(&m_mutex);
        m_report.addItem(item);
        stats = m_report.stats;
    }
    log(item);
    if (m_tracker)
        m_tracker->setCounts(stats.copied, stats.updated, stats.deleted, stats.errors);
}

RunStats ReportSink::stats() const
{
    QMutexLocker lock(&m_mutex);
    return m_report.stats;
}

void ReportSink::log(const ReportItem &item)
{
    if (!m_logger)
        return;
    if (item.kind == ItemKind::Error) {
        m_logger->error(QStringLiteral("%1 failed: %2 - %3 (attempts: %4)")
                            .arg(item.operation, item.relPath, item.message)
                            .arg(item.attempts));
        return;
    }
    QString line = QStringLiteral("%1 %2").arg(toString(item.kind).toUpper().leftJustified(14),
                                               item.relPath);
    if (item.size > 0)
        line += QStringLiteral("  (%1)").arg(QLocale().formattedDataSize(item.size));
    if (!item.message.isEmpty())
        line += QStringLiteral("  [%1]").arg(item.message);
    if (item.kind == ItemKind::Skipped)
        m_logger->detail(line);
    else
        m_logger->change(line);
}

} // namespace obor
