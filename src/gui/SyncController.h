#pragma once

#include "JobOptions.h"
#include "Progress.h"
#include "RunReport.h"
#include "SyncControl.h"
#include "SyncPlan.h"

#include <QObject>

#include <memory>

class QThread;

namespace obor {

using PlanPtr = std::shared_ptr<const SyncPlan>;
using ReportPtr = std::shared_ptr<const RunReport>;

// Runs a SyncEngine in a worker thread and relays its callbacks as queued
// signals, so the GUI stays responsive. Pause/cancel go straight to SyncControl.
class SyncController : public QObject
{
    Q_OBJECT

public:
    enum class StartMode { Run, Preview };

    explicit SyncController(QObject *parent = nullptr);
    ~SyncController() override;

    bool isRunning() const;
    bool isPaused() const { return m_control.isPaused(); }
    void start(const JobOptions &job, StartMode mode);
    void pause();
    void resume();
    void cancel();
    // Answer to confirmationRequested(); true = go ahead with the plan.
    void answerConfirmation(bool proceed);
    // Cancels a running sync and blocks until the worker thread has ended.
    void stopAndWait();

signals:
    void progressChanged(const obor::ProgressInfo &info);
    void logLine(const QString &line);
    void confirmationRequested(const obor::PlanPtr &plan);
    void finished(const obor::ReportPtr &report);

private:
    bool needsConfirmation(const SyncPlan &plan) const;
    void waitForThread();

    SyncControl m_control;
    QThread *m_thread = nullptr;
    StartMode m_mode = StartMode::Run;
    SyncMode m_syncMode = SyncMode::Update;
};

} // namespace obor

Q_DECLARE_METATYPE(obor::ProgressInfo)
Q_DECLARE_METATYPE(obor::PlanPtr)
Q_DECLARE_METATYPE(obor::ReportPtr)
