#include "MainWindow.h"

#include "AboutDialog.h"
#include "AdvancedOptionsPage.h"
#include "AppInfo.h"
#include "AppPaths.h"
#include "BasicOptionsPage.h"
#include "GuiUtil.h"
#include "HistoryDialog.h"
#include "IconFactory.h"
#include "JobListWidget.h"
#include "JobValidator.h"
#include "PathPicker.h"
#include "PreviewDialog.h"
#include "ProgressPanel.h"
#include "SummaryDialog.h"

#include <QActionGroup>
#include <QApplication>
#include <QCloseEvent>
#include <QHBoxLayout>
#include <QLabel>
#include <QMenuBar>
#include <QMessageBox>
#include <QPushButton>
#include <QScrollArea>
#include <QSplitter>
#include <QStackedWidget>
#include <QStatusBar>
#include <QTabWidget>
#include <QTimer>
#include <QVBoxLayout>

namespace obor {

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , m_store(AppPaths::jobsDir())
    , m_settings(AppSettings::load(AppPaths::settingsFile()))
    , m_sync(new SyncController(this))
    , m_saveTimer(new QTimer(this))
{
    setWindowTitle(AppInfo::name());
    setMinimumSize(980, 680);
    m_saveTimer->setSingleShot(true);
    m_saveTimer->setInterval(600);

    m_splitter = new QSplitter(Qt::Horizontal, this);
    m_splitter->addWidget(buildSidebar());
    m_splitter->addWidget(buildEditor());
    m_splitter->setStretchFactor(1, 1);
    m_splitter->setSizes({240, 900});
    m_splitter->setChildrenCollapsible(false);
    setCentralWidget(m_splitter);
    buildMenus();
    connectSignals();
    restoreSettings();
    m_jobs->reload(m_settings.lastJob);
}

QWidget *MainWindow::buildSidebar()
{
    auto *sidebar = new QFrame(this);
    sidebar->setObjectName(QStringLiteral("Sidebar"));
    sidebar->setMinimumWidth(200);
    auto *logo = new QLabel(sidebar);
    logo->setPixmap(IconFactory::appIcon().pixmap(36, 36));
    auto *brand = new QHBoxLayout;
    brand->addWidget(logo);
    brand->addWidget(GuiUtil::makeLabel(AppInfo::name(), QStringLiteral("AppTitle"), sidebar), 1);

    m_jobs = new JobListWidget(m_store, sidebar);
    auto *history = new QPushButton(IconFactory::icon(IconKind::History), tr("Run history"), sidebar);
    connect(history, &QPushButton::clicked, this, &MainWindow::showHistory);

    auto *layout = new QVBoxLayout(sidebar);
    layout->setContentsMargins(14, 16, 10, 14);
    layout->setSpacing(10);
    layout->addLayout(brand);
    layout->addSpacing(8);
    layout->addWidget(GuiUtil::makeLabel(tr("JOBS"), QStringLiteral("SidebarHeader"), sidebar));
    layout->addWidget(m_jobs, 1);
    layout->addWidget(history);
    return sidebar;
}

QWidget *MainWindow::buildEditor()
{
    auto *editor = new QWidget(this);
    m_basic = new BasicOptionsPage;
    m_advanced = new AdvancedOptionsPage;
    auto *advancedScroll = new QScrollArea;
    advancedScroll->setWidgetResizable(true);
    advancedScroll->setWidget(m_advanced);
    auto *basicScroll = new QScrollArea;
    basicScroll->setWidgetResizable(true);
    basicScroll->setWidget(m_basic);

    m_tabs = new QTabWidget;
    m_tabs->addTab(basicScroll, tr("Basic"));
    m_tabs->addTab(advancedScroll, tr("Advanced"));
    m_progress = new ProgressPanel;
    m_stack = new QStackedWidget;
    m_stack->addWidget(m_tabs);
    m_stack->addWidget(m_progress);

    auto *layout = new QVBoxLayout(editor);
    layout->setContentsMargins(20, 16, 20, 14);
    layout->setSpacing(12);
    layout->addWidget(buildJobHeader());
    layout->addWidget(m_stack, 1);
    layout->addWidget(buildActionBar());
    return editor;
}

QWidget *MainWindow::buildJobHeader()
{
    auto *card = GuiUtil::makeCard(this);
    m_jobTitle = GuiUtil::makeLabel(QString(), QStringLiteral("Title"), card);
    m_source = new PathPicker(tr("Source"), card);
    m_source->setPlaceholder(tr("Folder to copy from, e.g. D:\\Music"));
    m_dest = new PathPicker(tr("Destination"), card);
    m_dest->setPlaceholder(tr("Backup folder, e.g. E:\\Music-Backup"));
    auto *arrow = new QLabel(card);
    arrow->setPixmap(IconFactory::icon(IconKind::Arrow, Theme::colors().accent).pixmap(28, 28));

    auto *paths = new QHBoxLayout;
    paths->setSpacing(14);
    paths->addWidget(m_source, 1);
    paths->addWidget(arrow);
    paths->addWidget(m_dest, 1);
    auto *layout = new QVBoxLayout(card);
    layout->setContentsMargins(18, 14, 18, 12);
    layout->addWidget(m_jobTitle);
    layout->addLayout(paths);
    return card;
}

QWidget *MainWindow::buildActionBar()
{
    auto *bar = new QWidget(this);
    m_status = GuiUtil::makeLabel(QString(), QStringLiteral("Muted"), bar);
    m_previewButton = new QPushButton(IconFactory::icon(IconKind::Preview), tr("Preview"), bar);
    m_previewButton->setToolTip(tr("Scan and show what would change (F6)"));
    m_startButton = new QPushButton(IconFactory::icon(IconKind::Play, Qt::white), tr("Start sync"), bar);
    m_startButton->setObjectName(QStringLiteral("Primary"));
    m_startButton->setToolTip(tr("Start the sync (F5)"));
    m_pauseButton = new QPushButton(IconFactory::icon(IconKind::Pause), tr("Pause"), bar);
    m_cancelButton = new QPushButton(IconFactory::icon(IconKind::Stop), tr("Cancel"), bar);
    m_cancelButton->setObjectName(QStringLiteral("Danger"));
    auto *layout = new QHBoxLayout(bar);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(m_status, 1);
    for (QPushButton *b : {m_pauseButton, m_cancelButton, m_previewButton, m_startButton})
        layout->addWidget(b);
    m_pauseButton->setVisible(false);
    m_cancelButton->setVisible(false);
    return bar;
}

void MainWindow::buildMenus()
{
    QMenuBar *bar = menuBar();
    QMenu *file = bar->addMenu(tr("&File"));
    file->addAction(tr("&New job…"), QKeySequence::New, m_jobs, &JobListWidget::createJob);
    file->addAction(tr("&Save job"), QKeySequence::Save, this, [this]() { saveNow(); });
    file->addSeparator();
    file->addAction(tr("Run &history…"), QKeySequence(Qt::CTRL | Qt::Key_H), this, &MainWindow::showHistory);
    file->addAction(tr("Open &log folder"), this, &MainWindow::openLogFolder);
    file->addSeparator();
    file->addAction(tr("E&xit"), QKeySequence::Quit, this, &QWidget::close);
    buildRunMenu(bar);

    QMenu *view = bar->addMenu(tr("&View"));
    QMenu *theme = view->addMenu(tr("&Theme"));
    auto *group = new QActionGroup(this);
    const std::pair<ThemeMode, QString> themes[] = {{ThemeMode::System, tr("&System")},
                                                    {ThemeMode::Light, tr("&Light")},
                                                    {ThemeMode::Dark, tr("&Dark")}};
    for (const auto &[mode, label] : themes) {
        QAction *action = theme->addAction(label, this, [this, mode = mode]() { setTheme(mode); });
        action->setCheckable(true);
        action->setChecked(Theme::current() == mode);
        group->addAction(action);
    }

    QMenu *help = bar->addMenu(tr("&Help"));
    help->addAction(tr("&About Oborcopy"), this, [this]() { AboutDialog(this).exec(); });
    help->addAction(tr("About &Qt"), qApp, &QApplication::aboutQt);
}

void MainWindow::buildRunMenu(QMenuBar *bar)
{
    QMenu *run = bar->addMenu(tr("&Run"));
    m_startAction = run->addAction(tr("&Start sync"), QKeySequence(Qt::Key_F5), this,
                                   [this]() { startSync(SyncController::StartMode::Run); });
    m_previewAction = run->addAction(tr("&Preview"), QKeySequence(Qt::Key_F6), this,
                                     [this]() { startSync(SyncController::StartMode::Preview); });
    m_pauseAction = run->addAction(tr("P&ause / Resume"), QKeySequence(Qt::Key_F7), this, &MainWindow::togglePause);
    m_cancelAction = run->addAction(tr("&Cancel"), QKeySequence(Qt::SHIFT | Qt::Key_F5), this, &MainWindow::cancelSync);
    m_pauseAction->setEnabled(false);
    m_cancelAction->setEnabled(false);
}

void MainWindow::connectSignals()
{
    connect(m_jobs, &JobListWidget::aboutToModify, this, &MainWindow::saveNow);
    connect(m_jobs, &JobListWidget::jobSelected, this, &MainWindow::selectJob);
    connect(m_jobs, &JobListWidget::jobRenamed, this, &MainWindow::onJobRenamed);
    connect(m_jobs, &JobListWidget::jobDeleted, this, &MainWindow::onJobDeleted);
    connect(m_source, &PathPicker::pathChanged, this, &MainWindow::scheduleSave);
    connect(m_dest, &PathPicker::pathChanged, this, &MainWindow::scheduleSave);
    connect(m_basic, &BasicOptionsPage::changed, this, &MainWindow::scheduleSave);
    connect(m_advanced, &AdvancedOptionsPage::changed, this, &MainWindow::scheduleSave);
    connect(m_saveTimer, &QTimer::timeout, this, &MainWindow::saveNow);

    connect(m_previewButton, &QPushButton::clicked, m_previewAction, &QAction::trigger);
    connect(m_startButton, &QPushButton::clicked, m_startAction, &QAction::trigger);
    connect(m_pauseButton, &QPushButton::clicked, this, &MainWindow::togglePause);
    connect(m_cancelButton, &QPushButton::clicked, this, &MainWindow::cancelSync);

    connect(m_sync, &SyncController::progressChanged, m_progress, &ProgressPanel::updateProgress);
    connect(m_sync, &SyncController::logLine, m_progress, &ProgressPanel::appendLog);
    connect(m_sync, &SyncController::confirmationRequested, this, &MainWindow::onConfirmationRequested);
    connect(m_sync, &SyncController::finished, this, &MainWindow::onFinished);
    connect(m_progress, &ProgressPanel::detailsRequested, this, &MainWindow::showSummary);
    connect(m_progress, &ProgressPanel::openLogRequested, this, &MainWindow::openLastLog);
    connect(m_progress, &ProgressPanel::doneRequested, this, &MainWindow::showOptions);
}

// ---------------------------------------------------------------- jobs

void MainWindow::selectJob(const QString &name)
{
    if (m_hasJob && name == m_job.name)
        return;
    saveNow();
    loadJob(name);
}

void MainWindow::loadJob(const QString &name)
{
    const auto job = m_store.load(name);
    m_hasJob = job.has_value();
    if (!m_hasJob)
        return;
    m_loading = true;
    m_job = *job;
    m_jobTitle->setText(m_job.name);
    m_source->setPath(m_job.source);
    m_dest->setPath(m_job.destination);
    m_basic->load(m_job);
    m_advanced->load(m_job);
    m_loading = false;
    m_settings.lastJob = m_job.name;
    setWindowTitle(QStringLiteral("%1 – %2").arg(m_job.name, AppInfo::name()));
    showOptions();
}

void MainWindow::collectJob()
{
    m_job.source = m_source->path();
    m_job.destination = m_dest->path();
    m_basic->store(m_job);
    m_advanced->store(m_job);
}

void MainWindow::scheduleSave()
{
    if (!m_loading && m_hasJob)
        m_saveTimer->start();
}

void MainWindow::saveNow()
{
    m_saveTimer->stop();
    if (!m_hasJob)
        return;
    collectJob();
    QString error;
    if (m_store.save(m_job, &error))
        m_status->setText(tr("All changes saved"));
    else
        m_status->setText(tr("Could not save: %1").arg(error));
}

void MainWindow::onJobRenamed(const QString &oldName, const QString &newName)
{
    if (m_job.name != oldName)
        return;
    m_job.name = newName;
    m_jobTitle->setText(newName);
}

void MainWindow::onJobDeleted(const QString &name)
{
    if (m_job.name == name)
        m_hasJob = false;
}

// ---------------------------------------------------------------- running

void MainWindow::startSync(SyncController::StartMode mode)
{
    if (m_sync->isRunning() || !m_hasJob)
        return;
    saveNow();
    const QString problem = JobValidator::validate(m_job);
    if (!problem.isEmpty()) {
        QMessageBox::warning(this, tr("Cannot start"), problem);
        return;
    }
    m_previewRun = mode == SyncController::StartMode::Preview;
    m_progress->startRun(m_job.name, m_previewRun || m_job.copy.dryRun);
    m_stack->setCurrentWidget(m_progress);
    setRunning(true);
    m_sync->start(m_job, mode);
}

void MainWindow::onConfirmationRequested(const PlanPtr &plan)
{
    PreviewDialog dialog(*plan, m_job, m_previewRun, this);
    const bool proceed = dialog.exec() == QDialog::Accepted;
    if (proceed)
        m_previewRun = false;
    m_sync->answerConfirmation(proceed);
}

void MainWindow::onFinished(const ReportPtr &report)
{
    m_lastReport = report;
    setRunning(false);
    // A preview that was closed without starting changed nothing: go straight back.
    if (m_previewRun && report->status == RunStatus::Cancelled) {
        showOptions();
        m_status->setText(tr("Preview closed – nothing was changed."));
        return;
    }
    m_progress->showResult(*report);
    m_status->setText(report->summaryLine());
    QApplication::alert(this);
}

void MainWindow::togglePause()
{
    if (!m_sync->isRunning())
        return;
    const bool pause = !m_sync->isPaused();
    if (pause)
        m_sync->pause();
    else
        m_sync->resume();
    m_progress->setPaused(pause);
    m_pauseButton->setText(pause ? tr("Resume") : tr("Pause"));
    m_pauseButton->setIcon(IconFactory::icon(pause ? IconKind::Play : IconKind::Pause));
}

void MainWindow::cancelSync()
{
    if (!m_sync->isRunning())
        return;
    m_sync->cancel();
    m_progress->setPaused(false);
    m_cancelButton->setEnabled(false);
    m_pauseButton->setEnabled(false);
    m_status->setText(tr("Cancelling… the current file is cleaned up."));
}

void MainWindow::setRunning(bool running)
{
    m_jobs->setLocked(running);
    m_source->setEnabled(!running);
    m_dest->setEnabled(!running);
    m_previewButton->setVisible(!running);
    m_startButton->setVisible(!running);
    m_pauseButton->setVisible(running);
    m_cancelButton->setVisible(running);
    m_pauseButton->setEnabled(running);
    m_cancelButton->setEnabled(running);
    m_pauseButton->setText(tr("Pause"));
    m_pauseButton->setIcon(IconFactory::icon(IconKind::Pause));
    m_startAction->setEnabled(!running);
    m_previewAction->setEnabled(!running);
    m_pauseAction->setEnabled(running);
    m_cancelAction->setEnabled(running);
    m_status->setText(running ? tr("Sync running…") : QString());
}

void MainWindow::showOptions()
{
    if (!m_sync->isRunning())
        m_stack->setCurrentWidget(m_tabs);
}

// ---------------------------------------------------------------- misc

void MainWindow::showSummary()
{
    if (!m_lastReport)
        return;
    SummaryDialog dialog(*m_lastReport, this);
    dialog.exec();
}

void MainWindow::showHistory()
{
    QStringList dirs{AppPaths::defaultLogDir()};
    if (m_hasJob && !m_job.log.logDir.isEmpty())
        dirs << m_job.log.logDir;
    HistoryDialog dialog(dirs, this);
    dialog.exec();
}

void MainWindow::openLastLog()
{
    if (m_lastReport && !m_lastReport->logFile.isEmpty())
        GuiUtil::openFile(m_lastReport->logFile);
}

void MainWindow::openLogFolder()
{
    const QString dir = m_hasJob ? m_job.effectiveLogDir() : AppPaths::defaultLogDir();
    AppPaths::ensureDir(dir);
    GuiUtil::openInFileManager(dir);
}

void MainWindow::setTheme(ThemeMode mode)
{
    Theme::apply(mode);
    m_settings.theme = Theme::toString(mode);
}

void MainWindow::restoreSettings()
{
    if (!m_settings.windowGeometry.isEmpty())
        restoreGeometry(m_settings.windowGeometry);
    else
        resize(1180, 820);
    if (!m_settings.splitterState.isEmpty())
        m_splitter->restoreState(m_settings.splitterState);
}

void MainWindow::saveSettings()
{
    m_settings.windowGeometry = saveGeometry();
    m_settings.splitterState = m_splitter->saveState();
    m_settings.save(AppPaths::settingsFile());
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    if (m_sync->isRunning()) {
        const auto answer = QMessageBox::question(
            this, tr("Sync running"),
            tr("A sync is still running. Cancel it and quit?\n\nFiles that are already copied stay; "
               "the file being copied right now is discarded."));
        if (answer != QMessageBox::Yes) {
            event->ignore();
            return;
        }
        m_sync->stopAndWait();
    }
    saveNow();
    saveSettings();
    event->accept();
}

} // namespace obor
