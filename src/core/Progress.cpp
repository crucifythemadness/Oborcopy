#include "Progress.h"

#include "SyncControl.h"

namespace obor {

namespace {
constexpr qint64 kEmitIntervalMs = 100;
}

QString toString(SyncPhase phase)
{
    switch (phase) {
    case SyncPhase::Idle: return QStringLiteral("Ready");
    case SyncPhase::Validating: return QStringLiteral("Checking folders");
    case SyncPhase::Scanning: return QStringLiteral("Scanning folders");
    case SyncPhase::Comparing: return QStringLiteral("Comparing files");
    case SyncPhase::AwaitingConfirmation: return QStringLiteral("Waiting for confirmation");
    case SyncPhase::Deleting: return QStringLiteral("Deleting extra items");
    case SyncPhase::Copying: return QStringLiteral("Copying files");
    case SyncPhase::Finalizing: return QStringLiteral("Finishing");
    case SyncPhase::Finished: return QStringLiteral("Finished");
    }
    return {};
}

ProgressTracker::ProgressTracker(Callback callback, const SyncControl *control)
    : m_callback(std::move(callback))
    , m_control(control)
{
    m_emitTimer.start();
}

void ProgressTracker::setPhase(SyncPhase phase)
{
    QMutexLocker lock(&m_mutex);
    m_info.phase = phase;
    if (phase == SyncPhase::Copying) {
        m_copyTimer.start();
        m_pausedAtCopyStart = m_control ? m_control->pausedMs() : 0;
    }
    emitIfDue(lock, true);
}

void ProgressTracker::setScanned(int scanned)
{
    QMutexLocker lock(&m_mutex);
    m_info.scanned = scanned;
    emitIfDue(lock, false);
}

void ProgressTracker::setTotals(int files, qint64 bytes)
{
    QMutexLocker lock(&m_mutex);
    m_info.filesTotal = files;
    m_info.bytesTotal = bytes;
    emitIfDue(lock, true);
}

void ProgressTracker::beginFile(const QString &relPath, qint64 size)
{
    QMutexLocker lock(&m_mutex);
    m_info.currentFile = relPath;
    m_info.currentBytes = 0;
    m_info.currentSize = size;
    emitIfDue(lock, false);
}

void ProgressTracker::addBytes(qint64 delta)
{
    QMutexLocker lock(&m_mutex);
    m_info.bytesDone += delta;
    m_info.currentBytes = qMax<qint64>(0, m_info.currentBytes + delta);
    emitIfDue(lock, false);
}

void ProgressTracker::fileDone()
{
    QMutexLocker lock(&m_mutex);
    ++m_info.filesDone;
    emitIfDue(lock, false);
}

void ProgressTracker::setCounts(int copied, int updated, int deleted, int errors)
{
    QMutexLocker lock(&m_mutex);
    m_info.copied = copied;
    m_info.updated = updated;
    m_info.deleted = deleted;
    m_info.errors = errors;
    emitIfDue(lock, false);
}

void ProgressTracker::flush()
{
    QMutexLocker lock(&m_mutex);
    emitIfDue(lock, true);
}

ProgressInfo ProgressTracker::snapshot() const
{
    QMutexLocker lock(&m_mutex);
    return m_info;
}

void ProgressTracker::updateRate()
{
    if (!m_copyTimer.isValid())
        return;
    const qint64 paused = m_control ? m_control->pausedMs() - m_pausedAtCopyStart : 0;
    const qint64 activeMs = m_copyTimer.elapsed() - paused;
    if (activeMs < 500 || m_info.bytesDone <= 0)
        return;
    m_info.bytesPerSecond = double(m_info.bytesDone) * 1000.0 / double(activeMs);
    const qint64 remaining = qMax<qint64>(0, m_info.bytesTotal - m_info.bytesDone);
    m_info.etaSeconds = qint64(double(remaining) / m_info.bytesPerSecond);
}

void ProgressTracker::emitIfDue(QMutexLocker<QMutex> &lock, bool force)
{
    if (!m_callback || (!force && m_emitTimer.elapsed() < kEmitIntervalMs))
        return;
    m_emitTimer.restart();
    updateRate();
    const ProgressInfo copy = m_info;
    lock.unlock();
    m_callback(copy);
    lock.relock();
}

} // namespace obor
