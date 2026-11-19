#pragma once

#include "JobOptions.h"
#include "Progress.h"
#include "RunReport.h"
#include "Schedule.h"

#include <QCoreApplication>
#include <QString>

namespace obor {

// Translated GUI texts for things the core library describes in English.
// The core keeps English (log files, run reports, CLI and service output stay
// English); the GUI shows these instead.
class GuiText
{
    Q_DECLARE_TR_FUNCTIONS(GuiText)

public:
    static QString phase(SyncPhase phase);
    static QString itemKind(ItemKind kind);
    static QString status(RunStatus status);
    static QString mode(SyncMode mode);
    // e.g. "Weekly on Mon, Thu at 07:00"
    static QString describe(const ScheduleEntry &entry);
    // e.g. "1,204 files checked, 37 copied, 12 updated, 7 deleted, 2 errors"
    static QString summaryLine(const RunReport &report);

    // An English message from the core (validation, safety stops, disk checks,
    // file errors, change reasons, run status of a scheduled run) in the UI
    // language. Unknown text - e.g. an operating system error - is returned unchanged.
    static QString message(const QString &english);
};

} // namespace obor
