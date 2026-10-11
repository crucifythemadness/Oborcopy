#pragma once

#include <QElapsedTimer>
#include <QMutex>
#include <QString>

#include <functional>

namespace obor {

class SyncControl;

enum class SyncPhase {
    Idle,
    Validating,
    Scanning,
    Comparing,
    AwaitingConfirmation,
    Deleting,
    Copying,
    Finalizing,
    Finished
};

QString toString(SyncPhase phase);

struct ProgressInfo
{
    SyncPhase phase = SyncPhase::Idle;
    int scanned = 0;
    int filesDone = 0;
    int filesTotal = 0;
    qint64 bytesDone = 0;
    qint64 bytesTotal = 0;
    QString currentFile;
    qint64 currentBytes = 0;
    qint64 currentSize = 0;
    double bytesPerSecond = 0.0;
    qint64 etaSeconds = -1;
    int copied = 0;
    int updated = 0;
    int deleted = 0;
    int errors = 0;
};

// Collects progress from (possibly several) worker threads and reports it
// at most every 100 ms, so the UI is never flooded.
class ProgressTracker
{
public:
    using Callback = std::function<void(const ProgressInfo &)>;

    ProgressTracker(Callback callback, const SyncControl *control);

    void setPhase(SyncPhase phase);
    void setScanned(int scanned);
    void setTotals(int files, qint64 bytes);
    void beginFile(const QString &relPath, qint64 size);
    void addBytes(qint64 delta);
    void fileDone();
    void setCounts(int copied, int updated, int deleted, int errors);
    void flush();
    ProgressInfo snapshot() const;

private:
    void emitIfDue(QMutexLocker<QMutex> &lock, bool force);
    void updateRate();

    Callback m_callback;
    const SyncControl *m_control;
    mutable QMutex m_mutex;
    ProgressInfo m_info;
    QElapsedTimer m_emitTimer;
    QElapsedTimer m_copyTimer;
    qint64 m_pausedAtCopyStart = 0;
};

} // namespace obor
