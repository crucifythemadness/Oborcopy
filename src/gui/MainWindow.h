#pragma once

#include "AppSettings.h"
#include "JobOptions.h"
#include "JobStore.h"
#include "SyncController.h"
#include "Theme.h"

#include <QMainWindow>

class QAction;
class QLabel;
class QMenuBar;
class QPushButton;
class QSplitter;
class QStackedWidget;
class QTabWidget;
class QTimer;

namespace obor {

class AdvancedOptionsPage;
class BasicOptionsPage;
class JobListWidget;
class PathPicker;
class ProgressPanel;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);

protected:
    void closeEvent(QCloseEvent *event) override;

private:
    // UI construction
    QWidget *buildSidebar();
    QWidget *buildEditor();
    QWidget *buildJobHeader();
    QWidget *buildActionBar();
    void buildMenus();
    void buildRunMenu(QMenuBar *bar);
    void connectSignals();

    // Jobs
    void selectJob(const QString &name);
    void loadJob(const QString &name);
    void collectJob();
    void scheduleSave();
    void saveNow();
    void onJobRenamed(const QString &oldName, const QString &newName);
    void onJobDeleted(const QString &name);

    // Running
    void startSync(SyncController::StartMode mode);
    void onConfirmationRequested(const PlanPtr &plan);
    void onFinished(const ReportPtr &report);
    void togglePause();
    void cancelSync();
    void setRunning(bool running);
    void showOptions();

    // Misc
    void showSummary();
    void showHistory();
    void openLastLog();
    void openLogFolder();
    void setTheme(ThemeMode mode);
    void restoreSettings();
    void saveSettings();

    JobStore m_store;
    AppSettings m_settings;
    JobOptions m_job;
    bool m_hasJob = false;
    bool m_loading = false;
    bool m_previewRun = false;
    ReportPtr m_lastReport;
    SyncController *m_sync;
    QTimer *m_saveTimer;

    QSplitter *m_splitter = nullptr;
    JobListWidget *m_jobs = nullptr;
    QLabel *m_jobTitle = nullptr;
    PathPicker *m_source = nullptr;
    PathPicker *m_dest = nullptr;
    QTabWidget *m_tabs = nullptr;
    BasicOptionsPage *m_basic = nullptr;
    AdvancedOptionsPage *m_advanced = nullptr;
    QStackedWidget *m_stack = nullptr;
    ProgressPanel *m_progress = nullptr;
    QLabel *m_status = nullptr;
    QPushButton *m_previewButton = nullptr;
    QPushButton *m_startButton = nullptr;
    QPushButton *m_pauseButton = nullptr;
    QPushButton *m_cancelButton = nullptr;
    QAction *m_startAction = nullptr;
    QAction *m_previewAction = nullptr;
    QAction *m_pauseAction = nullptr;
    QAction *m_cancelAction = nullptr;
};

} // namespace obor
