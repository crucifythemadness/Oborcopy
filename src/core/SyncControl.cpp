#include "SyncControl.h"

#include <QDeadlineTimer>

namespace obor {

void SyncControl::reset()
{
    QMutexLocker lock(&m_mutex);
    m_paused = false;
    m_cancelled = false;
    m_decision = Decision::Pending;
    m_pausedTotalMs = 0;
    m_pauseTimer.invalidate();
}

void SyncControl::pause()
{
    QMutexLocker lock(&m_mutex);
    if (m_paused.exchange(true))
        return;
    m_pauseTimer.start();
}

void SyncControl::resume()
{
    QMutexLocker lock(&m_mutex);
    if (!m_paused.exchange(false))
        return;
    m_pausedTotalMs += m_pauseTimer.elapsed();
    m_pauseTimer.invalidate();
    m_wake.wakeAll();
}

void SyncControl::cancel()
{
    QMutexLocker lock(&m_mutex);
    m_cancelled = true;
    if (m_paused.exchange(false)) {
        m_pausedTotalMs += m_pauseTimer.elapsed();
        m_pauseTimer.invalidate();
    }
    m_wake.wakeAll();
}

bool SyncControl::checkpoint()
{
    if (!m_paused.load())
        return !m_cancelled.load();
    QMutexLocker lock(&m_mutex);
    while (m_paused.load() && !m_cancelled.load())
        m_wake.wait(&m_mutex);
    return !m_cancelled.load();
}

bool SyncControl::sleepFor(int ms)
{
    QDeadlineTimer deadline(ms);
    QMutexLocker lock(&m_mutex);
    while (!m_cancelled.load() && !deadline.hasExpired())
        m_wake.wait(&m_mutex, deadline);
    lock.unlock();
    return checkpoint();
}

qint64 SyncControl::pausedMs() const
{
    QMutexLocker lock(&m_mutex);
    return m_pausedTotalMs + (m_pauseTimer.isValid() ? m_pauseTimer.elapsed() : 0);
}

bool SyncControl::waitForDecision()
{
    QMutexLocker lock(&m_mutex);
    while (m_decision == Decision::Pending && !m_cancelled.load())
        m_wake.wait(&m_mutex);
    const bool proceed = m_decision == Decision::Proceed && !m_cancelled.load();
    m_decision = Decision::Pending;
    return proceed;
}

void SyncControl::decide(bool proceed)
{
    QMutexLocker lock(&m_mutex);
    m_decision = proceed ? Decision::Proceed : Decision::Abort;
    m_wake.wakeAll();
}

} // namespace obor
