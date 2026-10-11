#pragma once

#include <QElapsedTimer>
#include <QMutex>

namespace obor {

class SyncControl;

// Limits the combined write throughput of all copy threads.
class Throttle
{
public:
    explicit Throttle(int megabytesPerSecond);

    bool enabled() const { return m_bytesPerSecond > 0; }
    // Accounts for 'bytes' and sleeps if the run is ahead of the allowed rate.
    void consume(qint64 bytes, SyncControl &control);

private:
    qint64 m_bytesPerSecond;
    QMutex m_mutex;
    QElapsedTimer m_timer;
    qint64 m_total = 0;
};

} // namespace obor
