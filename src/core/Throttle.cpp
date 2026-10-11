#include "Throttle.h"

#include "SyncControl.h"

namespace obor {

Throttle::Throttle(int megabytesPerSecond)
    : m_bytesPerSecond(qint64(qMax(0, megabytesPerSecond)) * 1024 * 1024)
{
}

void Throttle::consume(qint64 bytes, SyncControl &control)
{
    if (!enabled())
        return;
    qint64 waitMs = 0;
    {
        QMutexLocker lock(&m_mutex);
        if (!m_timer.isValid())
            m_timer.start();
        m_total += bytes;
        const qint64 expectedMs = m_total * 1000 / m_bytesPerSecond;
        const qint64 activeMs = m_timer.elapsed() - control.pausedMs();
        waitMs = expectedMs - activeMs;
    }
    if (waitMs > 0)
        control.sleepFor(int(qMin<qint64>(waitMs, 2000)));
}

} // namespace obor
