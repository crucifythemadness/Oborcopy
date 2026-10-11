#include "RetryPolicy.h"
#include "SyncControl.h"

#include <QElapsedTimer>
#include <QTest>
#include <QThread>

using namespace obor;

class TestRetryPolicy : public QObject
{
    Q_OBJECT

private slots:
    void exponentialSchedule()
    {
        const RetryPolicy policy({5, 1000, 60000});
        QCOMPARE(policy.delayMs(1, 0.0), 1000);
        QCOMPARE(policy.delayMs(2, 0.0), 2000);
        QCOMPARE(policy.delayMs(3, 0.0), 4000);
        QCOMPARE(policy.delayMs(4, 0.0), 8000);
    }

    void cappedAtMaximum()
    {
        const RetryPolicy policy({10, 2000, 10000});
        QCOMPARE(policy.delayMs(5, 0.0), 10000);
        QCOMPARE(policy.delayMs(30, 0.0), 10000);
    }

    void jitterBand()
    {
        const RetryPolicy policy({3, 1000, 60000});
        QCOMPARE(policy.delayMs(1, 1.0), 1200);
        QCOMPARE(policy.delayMs(1, -1.0), 800);
        for (int i = 0; i < 100; ++i) {
            const int d = policy.delayMs(2);
            QVERIFY(d >= 1600 && d <= 2400);
        }
    }

    void sleepIsInterruptedByCancel()
    {
        SyncControl control;
        QThread *canceller = QThread::create([&control]() {
            QThread::msleep(50);
            control.cancel();
        });
        canceller->start();
        QElapsedTimer timer;
        timer.start();
        QVERIFY(!control.sleepFor(10000));
        QVERIFY(timer.elapsed() < 5000);
        canceller->wait();
        delete canceller;
    }

    void pauseBlocksCheckpoint()
    {
        SyncControl control;
        control.pause();
        QThread *resumer = QThread::create([&control]() {
            QThread::msleep(100);
            control.resume();
        });
        resumer->start();
        QElapsedTimer timer;
        timer.start();
        QVERIFY(control.checkpoint());
        QVERIFY(timer.elapsed() >= 80);
        QVERIFY(control.pausedMs() >= 80);
        resumer->wait();
        delete resumer;
    }
};

QTEST_GUILESS_MAIN(TestRetryPolicy)
#include "tst_retrypolicy.moc"
