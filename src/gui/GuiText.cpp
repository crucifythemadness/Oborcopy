#include "GuiText.h"

#include "Language.h"

#include <QHash>
#include <QLocale>
#include <QMutex>
#include <QRegularExpression>
#include <QStringList>
#include <QVector>

#include <algorithm>

namespace obor {

namespace {

// English messages produced by the core library (see the files named in the
// comments). The text must stay identical to the core's; "%1".."%3" stand for
// the parts that vary (paths, names, operating system errors). Each part is
// itself translated when it is a known message (e.g. "Source").
const char *const kCoreMessages[] = {
    // JobValidator.cpp
    QT_TRANSLATE_NOOP("CoreMessage", "Please choose a source folder."),
    QT_TRANSLATE_NOOP("CoreMessage", "Please choose a destination folder."),
    QT_TRANSLATE_NOOP("CoreMessage", "The source folder does not exist: %1"),
    QT_TRANSLATE_NOOP("CoreMessage", "The destination is not a folder: %1"),
    QT_TRANSLATE_NOOP("CoreMessage", "The destination must not be the source folder or lie inside it."),
    QT_TRANSLATE_NOOP("CoreMessage", "The source must not lie inside the destination folder."),
    // SyncEngine.cpp
    QT_TRANSLATE_NOOP("CoreMessage", "Source"),
    QT_TRANSLATE_NOOP("CoreMessage", "Destination"),
    QT_TRANSLATE_NOOP("CoreMessage", "%1: several connected disks are named %2. Disconnect all but one and try again."),
    QT_TRANSLATE_NOOP("CoreMessage", "%1: %2 is a different disk, not %3. Connect %3, or choose the folder again in Oborcopy to use this disk."),
    QT_TRANSLATE_NOOP("CoreMessage", "%1: the disk %2 is not connected."),
    QT_TRANSLATE_NOOP("CoreMessage", "The source folder is empty but the destination is not. Mirroring would delete everything in the destination, so the run was stopped."),
    QT_TRANSLATE_NOOP("CoreMessage", "Cannot create the destination folder %1."),
    // SyncEngine.cpp / SyncExecutor.cpp: operations in the error list
    QT_TRANSLATE_NOOP("CoreMessage", "Scan"),
    QT_TRANSLATE_NOOP("CoreMessage", "Copy"),
    QT_TRANSLATE_NOOP("CoreMessage", "Update"),
    QT_TRANSLATE_NOOP("CoreMessage", "Delete"),
    QT_TRANSLATE_NOOP("CoreMessage", "Archive"),
    QT_TRANSLATE_NOOP("CoreMessage", "Delete folder"),
    QT_TRANSLATE_NOOP("CoreMessage", "Create folder"),
    QT_TRANSLATE_NOOP("CoreMessage", "Remove source (move)"),
    // FileCopier.cpp, FileScanner.cpp, SyncExecutor.cpp
    QT_TRANSLATE_NOOP("CoreMessage", "Source file no longer exists"),
    QT_TRANSLATE_NOOP("CoreMessage", "Cannot open source: %1"),
    QT_TRANSLATE_NOOP("CoreMessage", "Cannot create folder %1"),
    QT_TRANSLATE_NOOP("CoreMessage", "Cannot create target: %1"),
    QT_TRANSLATE_NOOP("CoreMessage", "Read error: %1"),
    QT_TRANSLATE_NOOP("CoreMessage", "Write error: %1"),
    QT_TRANSLATE_NOOP("CoreMessage", "Cannot set file time: %1"),
    QT_TRANSLATE_NOOP("CoreMessage", "Verification read failed: %1"),
    QT_TRANSLATE_NOOP("CoreMessage", "Verification failed: checksum mismatch"),
    QT_TRANSLATE_NOOP("CoreMessage", "Cannot replace target: %1"),
    QT_TRANSLATE_NOOP("CoreMessage", "Folder is not readable"),
    QT_TRANSLATE_NOOP("CoreMessage", "Cannot remove folder"),
    QT_TRANSLATE_NOOP("CoreMessage", "Cannot create folder"),
    // SyncPlanner.cpp: why an item is copied, deleted or skipped
    QT_TRANSLATE_NOOP("CoreMessage", "new file"),
    QT_TRANSLATE_NOOP("CoreMessage", "new folder"),
    QT_TRANSLATE_NOOP("CoreMessage", "always overwrite"),
    QT_TRANSLATE_NOOP("CoreMessage", "unchanged"),
    QT_TRANSLATE_NOOP("CoreMessage", "content differs"),
    QT_TRANSLATE_NOOP("CoreMessage", "source newer"),
    QT_TRANSLATE_NOOP("CoreMessage", "source older"),
    QT_TRANSLATE_NOOP("CoreMessage", "size changed"),
    QT_TRANSLATE_NOOP("CoreMessage", "not in source"),
    // JobStore.cpp
    QT_TRANSLATE_NOOP("CoreMessage", "Job '%1' not found in %2"),
    QT_TRANSLATE_NOOP("CoreMessage", "A job named '%1' already exists"),
    QT_TRANSLATE_NOOP("CoreMessage", "Cannot open %1: %2"),
    QT_TRANSLATE_NOOP("CoreMessage", "Invalid JSON in %1: %2"),
    QT_TRANSLATE_NOOP("CoreMessage", "Cannot write %1: %2"),
    QT_TRANSLATE_NOOP("CoreMessage", "Cannot save %1: %2"),
    // RunReport.cpp / service/Scheduler.cpp: result of a (scheduled) run
    QT_TRANSLATE_NOOP("CoreMessage", "Completed"),
    QT_TRANSLATE_NOOP("CoreMessage", "Completed with errors"),
    QT_TRANSLATE_NOOP("CoreMessage", "Cancelled"),
    QT_TRANSLATE_NOOP("CoreMessage", "Failed"),
    QT_TRANSLATE_NOOP("CoreMessage", "Failed to start"),
    QT_TRANSLATE_NOOP("CoreMessage", "Crashed"),
};

// SyncEngine.cpp elsewhereHint(): appended to other messages.
const char *const kElsewhereHint =
    QT_TRANSLATE_NOOP("CoreMessage", " The folder exists on %1 as %2 – open the job in Oborcopy to use it.");

struct Pattern
{
    QRegularExpression regex;
    QVector<int> argOfGroup; // capture group i+1 holds placeholder argOfGroup[i]
    QString translated;
};

// "%1: %2 is ... not %3. Connect %3" -> ^(.*?): (.*?) is ... not (.*?)\. Connect \3$
Pattern makePattern(const char *source)
{
    Pattern p;
    p.translated = QCoreApplication::translate("CoreMessage", source);
    const QString text = QString::fromUtf8(source);
    static const QRegularExpression placeholder(QStringLiteral("%(\\d)"));
    QString regex = QStringLiteral("^");
    QHash<int, int> groupOfArg;
    qsizetype pos = 0;
    for (auto it = placeholder.globalMatch(text); it.hasNext();) {
        const QRegularExpressionMatch m = it.next();
        regex += QRegularExpression::escape(text.mid(pos, m.capturedStart() - pos));
        const int arg = m.captured(1).toInt();
        if (groupOfArg.contains(arg)) {
            regex += QStringLiteral("\\%1").arg(groupOfArg.value(arg));
        } else {
            p.argOfGroup.append(arg);
            groupOfArg.insert(arg, int(p.argOfGroup.size()));
            regex += QStringLiteral("(.*?)");
        }
        pos = m.capturedEnd();
    }
    regex += QRegularExpression::escape(text.mid(pos)) + QStringLiteral("$");
    p.regex = QRegularExpression(regex, QRegularExpression::DotMatchesEverythingOption);
    return p;
}

// Replaces %1..%9 in one pass, so a value that itself contains "%2" stays as it is.
QString fill(const QString &text, const QHash<int, QString> &args)
{
    QString result;
    result.reserve(text.size() + 64);
    for (qsizetype i = 0; i < text.size(); ++i) {
        if (text.at(i) == u'%' && i + 1 < text.size() && text.at(i + 1).isDigit()) {
            const int arg = text.at(i + 1).digitValue();
            if (args.contains(arg)) {
                result += args.value(arg);
                ++i;
                continue;
            }
        }
        result += text.at(i);
    }
    return result;
}

class MessageTranslator
{
public:
    MessageTranslator()
    {
        for (const char *source : kCoreMessages)
            m_patterns.append(makePattern(source));
        m_hint = makePattern(kElsewhereHint);
    }

    QString translate(const QString &english, int depth = 0)
    {
        if (english.isEmpty() || depth > 3)
            return english;
        QMutexLocker lock(&m_mutex);
        const auto cached = m_cache.constFind(english);
        if (cached != m_cache.constEnd())
            return *cached;
        lock.unlock();

        QString result = translateUncached(english, depth);
        lock.relock();
        if (m_cache.size() > 20000)
            m_cache.clear();
        m_cache.insert(english, result);
        return result;
    }

private:
    QString translateUncached(const QString &english, int depth)
    {
        // "<message>[.] The folder exists on X as Y – open the job ..." (SyncEngine elsewhereHint)
        const qsizetype hintAt = english.indexOf(QLatin1String(" The folder exists on "));
        if (hintAt > 0) {
            QString head = english.left(hintAt);
            const QString hint = english.mid(hintAt);
            QString tail = hint;
            if (const auto m = m_hint.regex.match(hint); m.hasMatch())
                tail = apply(m_hint, m, depth);
            return translate(head, depth + 1) + tail;
        }
        if (QString hit; match(english, depth, &hit))
            return hit;
        // The engine adds a full stop after a message that has none ("…: path" + ".").
        if (english.endsWith(u'.')) {
            if (QString hit; match(english.chopped(1), depth, &hit))
                return hit + u'.';
        } else if (QString hit; match(english + u'.', depth, &hit)) {
            return hit.endsWith(u'.') ? hit.chopped(1) : hit;
        }
        return english;
    }

    bool match(const QString &english, int depth, QString *result)
    {
        for (const Pattern &p : std::as_const(m_patterns)) {
            const QRegularExpressionMatch m = p.regex.match(english);
            if (m.hasMatch()) {
                *result = apply(p, m, depth);
                return true;
            }
        }
        return false;
    }

    QString apply(const Pattern &p, const QRegularExpressionMatch &m, int depth)
    {
        // Only the side ("Source:"/"Destination:") is translated inside a message;
        // paths and names are kept, even if a folder happens to be called "Copy".
        QHash<int, QString> args;
        for (int group = 1; group <= p.argOfGroup.size(); ++group) {
            const QString part = m.captured(group);
            const bool side = part == QLatin1String("Source") || part == QLatin1String("Destination");
            args.insert(p.argOfGroup.at(group - 1), side ? translate(part, depth + 1) : part);
        }
        return fill(p.translated, args);
    }

    QVector<Pattern> m_patterns;
    Pattern m_hint;
    QMutex m_mutex;
    QHash<QString, QString> m_cache;
};

} // namespace

QString GuiText::phase(SyncPhase phase)
{
    switch (phase) {
    case SyncPhase::Idle: return tr("Ready");
    case SyncPhase::Validating: return tr("Checking folders");
    case SyncPhase::Scanning: return tr("Scanning folders");
    case SyncPhase::Comparing: return tr("Comparing files");
    case SyncPhase::AwaitingConfirmation: return tr("Waiting for confirmation");
    case SyncPhase::Deleting: return tr("Deleting extra items");
    case SyncPhase::Copying: return tr("Copying files");
    case SyncPhase::Finalizing: return tr("Finishing");
    case SyncPhase::Finished: return tr("Finished");
    }
    return {};
}

QString GuiText::itemKind(ItemKind kind)
{
    switch (kind) {
    case ItemKind::Copied: return tr("Copied");
    case ItemKind::Updated: return tr("Updated");
    case ItemKind::Deleted: return tr("Deleted");
    case ItemKind::DirCreated: return tr("Folder created");
    case ItemKind::DirDeleted: return tr("Folder deleted");
    case ItemKind::Skipped: return tr("Unchanged");
    case ItemKind::Error: return tr("Error");
    }
    return {};
}

QString GuiText::status(RunStatus status)
{
    switch (status) {
    case RunStatus::Completed: return tr("Completed");
    case RunStatus::CompletedWithErrors: return tr("Completed with errors");
    case RunStatus::Cancelled: return tr("Cancelled");
    case RunStatus::Failed: return tr("Failed");
    }
    return {};
}

QString GuiText::mode(SyncMode mode)
{
    return mode == SyncMode::Mirror ? tr("Mirror") : tr("Update", "sync mode");
}

QString GuiText::describe(const ScheduleEntry &e)
{
    const QLocale locale;
    const QString at = locale.toString(e.time, QStringLiteral("HH:mm"));
    const QString minute = QStringLiteral("%1").arg(e.time.minute(), 2, 10, QLatin1Char('0'));
    switch (e.frequency) {
    case Frequency::Hourly:
        if (e.intervalHours <= 1)
            return tr("Every hour at :%1").arg(minute);
        return tr("Every %1 hours at :%2").arg(e.intervalHours).arg(minute);
    case Frequency::Daily:
        return tr("Daily at %1").arg(at);
    case Frequency::Weekly: {
        if (e.weekdays.size() == 7)
            return tr("Daily at %1").arg(at);
        QList<int> days = e.weekdays;
        std::sort(days.begin(), days.end());
        QStringList names;
        for (int day : std::as_const(days))
            names << (Language::isGerman() ? locale : QLocale(QLocale::English))
                         .dayName(day, QLocale::ShortFormat);
        return tr("Weekly on %1 at %2").arg(names.join(QStringLiteral(", ")), at);
    }
    case Frequency::Monthly:
        if (e.dayOfMonth <= 0)
            return tr("Monthly on the last day at %1").arg(at);
        return tr("Monthly on day %1 at %2").arg(e.dayOfMonth).arg(at);
    }
    return {};
}

QString GuiText::summaryLine(const RunReport &report)
{
    const QLocale locale;
    auto n = [&locale](int value) { return locale.toString(value); };
    const RunStats &s = report.stats;
    if (report.dryRun) {
        return tr("Preview: %1 files checked, %2 to copy, %3 to update, %4 to delete")
            .arg(n(s.filesChecked), n(s.copied), n(s.updated), n(s.deleted));
    }
    return tr("%1 files checked, %2 copied, %3 updated, %4 deleted, %5 errors")
        .arg(n(s.filesChecked), n(s.copied), n(s.updated), n(s.deleted), n(s.errors));
}

QString GuiText::message(const QString &english)
{
    if (!Language::isGerman())
        return english;
    static MessageTranslator translator; // built after the translators are installed
    return translator.translate(english);
}

} // namespace obor
