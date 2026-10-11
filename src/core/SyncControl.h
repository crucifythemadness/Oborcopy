#pragma once

#include <QElapsedTimer>
#include <QMutex>
#include <QWaitCondition>

#include <atomic>

namespace obor {

// Thread-safe pause/cancel/confirm channel between the UI and the worker thread.
// All worker loops call checkpoint() regularly; it blocks while paused and
// returns false once the run was cancelled.
class SyncControl
{
public:
    void reset();

    void pause();
    void resume();
    void cancel();
    bool isPaused() const { return m_paused.load(); }
    bool isCancelled() const { return m_cancelled.load(); }

    bool checkpoint();
    // Sleeps up to 'ms' milliseconds; returns false if cancelled meanwhile.
    bool sleepFor(int ms);
    // Total time spent paused, so speed/ETA can ignore it.
    qint64 pausedMs() const;

    // Worker side: blocks until the user decided (or cancelled). True = proceed.
    bool waitForDecision();
    // UI side: answers a pending waitForDecision().
    void decide(bool proceed);

private:
    enum class Decision { Pending, Proceed, Abort };

    mutable QMutex m_mutex;
    QWaitCondition m_wake;
    std::atomic_bool m_paused{false};
    std::atomic_bool m_cancelled{false};
    Decision m_decision = Decision::Pending;
    QElapsedTimer m_pauseTimer;
    qint64 m_pausedTotalMs = 0;
};

} // namespace obor
