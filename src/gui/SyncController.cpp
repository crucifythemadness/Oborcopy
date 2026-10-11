#include "SyncController.h"

#include "SyncEngine.h"

#include <QThread>

namespace obor {

SyncController::SyncController(QObject *parent)
    : QObject(parent)
{
    qRegisterMetaType<obor::ProgressInfo>();
    qRegisterMetaType<obor::PlanPtr>();
    qRegisterMetaType<obor::ReportPtr>();
}

SyncController::~SyncController()
{
    m_control.cancel();
    waitForThread();
}

bool SyncController::isRunning() const
{
    return m_thread && m_thread->isRunning();
}

void SyncController::start(const JobOptions &job, StartMode mode)
{
    waitForThread();
    m_control.reset();
    m_mode = mode;
    m_syncMode = job.mode;

    SyncCallbacks callbacks;
    callbacks.progress = [this](const ProgressInfo &info) { emit progressChanged(info); };
    callbacks.log = [this](const QString &line) { emit logLine(line); };
    callbacks.confirm = [this](const SyncPlan &plan) {
        if (!needsConfirmation(plan))
            return true;
        emit confirmationRequested(std::make_shared<const SyncPlan>(plan));
        return m_control.waitForDecision();
    };

    m_thread = QThread::create([this, job, callbacks]() {
        SyncEngine engine(job, m_control, callbacks);
        auto report = std::make_shared<const RunReport>(engine.run());
        emit finished(report);
    });
    m_thread->start();
}

bool SyncController::needsConfirmation(const SyncPlan &plan) const
{
    if (m_mode == StartMode::Preview)
        return true;
    return m_syncMode == SyncMode::Mirror && (plan.hasDeletions() || plan.thresholdExceeded);
}

void SyncController::pause()
{
    m_control.pause();
}

void SyncController::resume()
{
    m_control.resume();
}

void SyncController::cancel()
{
    m_control.cancel();
}

void SyncController::answerConfirmation(bool proceed)
{
    m_control.decide(proceed);
}

void SyncController::stopAndWait()
{
    m_control.cancel();
    waitForThread();
}

void SyncController::waitForThread()
{
    if (!m_thread)
        return;
    m_thread->wait();
    delete m_thread;
    m_thread = nullptr;
}

} // namespace obor
