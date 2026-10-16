#include "ServiceDialog.h"

#include "AppInfo.h"
#include "GuiUtil.h"
#include "HistoryDialog.h"
#include "IconFactory.h"
#include "JobStore.h"
#include "RunReport.h"
#include "ScheduleEditorDialog.h"
#include "SummaryDialog.h"
#include "Theme.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QJsonDocument>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QTableWidget>
#include <QVBoxLayout>

namespace obor {

namespace {

enum Column { ColOn, ColJob, ColSchedule, ColNext, ColLast, ColResult, ColumnCount };

QPushButton *button(IconKind icon, const QString &text, QWidget *parent)
{
    return new QPushButton(IconFactory::icon(icon), text, parent);
}

QColor statusColor(const QString &status)
{
    const ThemeColors &c = Theme::colors();
    if (status == QLatin1String("Completed"))
        return c.success;
    if (status == QLatin1String("Cancelled"))
        return c.muted;
    if (status.isEmpty())
        return c.text;
    return status == QLatin1String("Completed with errors") ? c.warning : c.danger;
}

} // namespace

ServiceDialog::ServiceDialog(ServiceController *controller, JobStore &store, QWidget *parent)
    : QDialog(parent)
    , m_controller(controller)
    , m_store(store)
    , m_schedule(ServiceSchedule::load(ServiceInfo::schedulePath()))
{
    setWindowTitle(tr("Service Manager"));
    resize(900, 640);

    auto *close = new QPushButton(tr("Close"), this);
    auto *footer = new QHBoxLayout;
    footer->addWidget(GuiUtil::makeLabel(tr("The service keeps running after you close Oborcopy."),
                                         QStringLiteral("Muted"), this),
                      1);
    footer->addWidget(close);

    auto *layout = new QVBoxLayout(this);
    layout->setSpacing(12);
    layout->addWidget(buildStatusCard());
    layout->addWidget(buildScheduleCard(), 1);
    layout->addLayout(footer);

    connect(close, &QPushButton::clicked, this, &QDialog::accept);
    connect(m_controller, &ServiceController::stateChanged, this, &ServiceDialog::updateState);
    connect(m_controller, &ServiceController::actionFinished, this, &ServiceDialog::onActionFinished);
    refreshTable();
    updateState(m_controller->state());
    m_controller->refresh();
}

QWidget *ServiceDialog::buildStatusCard()
{
    auto *card = GuiUtil::makeCard(this);
    m_dot = new QLabel(card);
    m_title = GuiUtil::makeLabel(QString(), QStringLiteral("Title"), card);
    m_details = GuiUtil::makeLabel(QString(), QStringLiteral("Muted"), card);
    m_details->setWordWrap(true);
    m_details->setTextInteractionFlags(Qt::TextSelectableByMouse);
    m_message = new QLabel(card);
    m_message->setWordWrap(true);
    m_message->setVisible(false);
    m_fix = new QPushButton(card);
    m_fix->setVisible(false);

    m_install = button(IconKind::Add, tr("Install service…"), card);
    m_install->setObjectName(QStringLiteral("Primary"));
    m_start = button(IconKind::Play, tr("Start"), card);
    m_pause = button(IconKind::Pause, tr("Pause"), card);
    m_stop = button(IconKind::Stop, tr("Stop"), card);
    m_restart = button(IconKind::MirrorMode, tr("Restart"), card);
    m_password = button(IconKind::Edit, tr("Update password…"), card);
    m_uninstall = button(IconKind::Trash, tr("Uninstall"), card);
    m_uninstall->setObjectName(QStringLiteral("Danger"));
    m_password->setVisible(ServiceController::supportsPasswordUpdate());

    auto *titleRow = new QHBoxLayout;
    titleRow->addWidget(m_dot);
    titleRow->addWidget(m_title, 1);
    auto *messageRow = new QHBoxLayout;
    messageRow->addWidget(m_message, 1);
    messageRow->addWidget(m_fix);
    auto *buttons = new QHBoxLayout;
    for (QPushButton *b : {m_install, m_start, m_pause, m_stop, m_restart})
        buttons->addWidget(b);
    buttons->addStretch();
    buttons->addWidget(m_password);
    buttons->addWidget(m_uninstall);

    auto *layout = new QVBoxLayout(card);
    layout->setContentsMargins(18, 14, 18, 14);
    layout->setSpacing(8);
    layout->addLayout(titleRow);
    layout->addWidget(m_details);
    layout->addLayout(messageRow);
    layout->addLayout(buttons);

    connect(m_install, &QPushButton::clicked, m_controller, &ServiceController::install);
    connect(m_start, &QPushButton::clicked, m_controller, &ServiceController::start);
    connect(m_stop, &QPushButton::clicked, m_controller, &ServiceController::stop);
    connect(m_restart, &QPushButton::clicked, m_controller, &ServiceController::restart);
    connect(m_password, &QPushButton::clicked, m_controller, &ServiceController::updatePassword);
    connect(m_uninstall, &QPushButton::clicked, this, &ServiceDialog::confirmUninstall);
    connect(m_fix, &QPushButton::clicked, this, &ServiceDialog::fixProblem);
    connect(m_pause, &QPushButton::clicked, this, [this]() {
        if (m_controller->state().kind == ServiceState::Kind::Paused)
            m_controller->resume();
        else
            m_controller->pause();
    });
    return card;
}

QWidget *ServiceDialog::buildScheduleCard()
{
    auto *card = GuiUtil::makeCard(this);
    m_table = new QTableWidget(0, ColumnCount, card);
    m_table->setHorizontalHeaderLabels({tr("On"), tr("Job"), tr("Schedule"), tr("Next run"), tr("Last run"), tr("Result")});
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->verticalHeader()->setVisible(false);
    m_table->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(ColResult, QHeaderView::Stretch);
    m_table->setAlternatingRowColors(true);
    m_hint = GuiUtil::makeLabel(QString(), QStringLiteral("Muted"), card);
    m_hint->setWordWrap(true);

    auto *add = button(IconKind::Add, tr("Schedule a job…"), card);
    m_edit = button(IconKind::Edit, tr("Edit…"), card);
    m_remove = button(IconKind::Trash, tr("Remove"), card);
    m_lastRun = button(IconKind::Document, tr("Last run details"), card);
    auto *history = button(IconKind::History, tr("Scheduled run history"), card);
    auto *log = button(IconKind::Document, tr("Service log"), card);
    log->setToolTip(QDir::toNativeSeparators(ServiceInfo::logPath()));

    auto *buttons = new QHBoxLayout;
    for (QPushButton *b : {add, m_edit, m_remove, m_lastRun})
        buttons->addWidget(b);
    buttons->addStretch();
    buttons->addWidget(history);
    buttons->addWidget(log);

    auto *layout = new QVBoxLayout(card);
    layout->setContentsMargins(18, 14, 18, 14);
    layout->addWidget(GuiUtil::makeLabel(tr("Scheduled jobs"), QStringLiteral("SectionTitle"), card));
    layout->addWidget(m_table, 1);
    layout->addWidget(m_hint);
    layout->addLayout(buttons);

    connect(add, &QPushButton::clicked, this, &ServiceDialog::addEntry);
    connect(m_edit, &QPushButton::clicked, this, &ServiceDialog::editEntry);
    connect(m_remove, &QPushButton::clicked, this, &ServiceDialog::removeEntry);
    connect(m_lastRun, &QPushButton::clicked, this, &ServiceDialog::showLastRun);
    connect(history, &QPushButton::clicked, this, &ServiceDialog::showScheduledHistory);
    connect(log, &QPushButton::clicked, this, []() {
        if (QFileInfo::exists(ServiceInfo::logPath()))
            GuiUtil::openFile(ServiceInfo::logPath());
    });
    connect(m_table, &QTableWidget::cellDoubleClicked, this, [this](int row, int column) {
        if (column != ColOn)
            editEntryAt(row);
    });
    connect(m_table, &QTableWidget::itemChanged, this, &ServiceDialog::onItemChanged);
    connect(m_table, &QTableWidget::itemSelectionChanged, this, &ServiceDialog::updateSelectionButtons);
    return card;
}

void ServiceDialog::updateSelectionButtons()
{
    const int i = selectedIndex();
    m_edit->setEnabled(i >= 0);
    m_remove->setEnabled(i >= 0);
    m_lastRun->setEnabled(i >= 0 && QFileInfo::exists(m_runState.jobs.value(m_schedule.entries.at(i).job).lastReport));
}

// ---------------------------------------------------------------- status

void ServiceDialog::updateState(const ServiceState &state)
{
    const ThemeColors &c = Theme::colors();
    const bool busy = m_controller->isBusy();
    if (busy)
        m_actionMessage.clear(); // a new action started: the old result is outdated
    QColor color = c.muted;
    switch (state.kind) {
    case ServiceState::Kind::Running: color = state.responding ? c.success : c.warning; break;
    case ServiceState::Kind::Busy: color = c.accent; break;
    case ServiceState::Kind::Paused:
    case ServiceState::Kind::Starting:
    case ServiceState::Kind::Stopping: color = c.warning; break;
    case ServiceState::Kind::Failed: color = c.danger; break;
    default: break;
    }
    m_dot->setPixmap(IconFactory::dot(color).pixmap(22, 22));
    m_title->setText(busy ? m_controller->busyText() : state.title());

    QStringList details;
    switch (state.kind) {
    case ServiceState::Kind::Unavailable:
        details << state.message;
        break;
    case ServiceState::Kind::NotInstalled:
#ifdef Q_OS_WIN
        details << tr("Install the Oborcopy Scheduler to run your jobs at set times – also when Oborcopy is "
                      "closed. Windows asks once for administrator permission and for your Windows password: "
                      "the service runs under your account, so it reaches the same folders and network "
                      "shares as you.");
#else
        details << tr("Install the Oborcopy Scheduler to run your jobs at set times – also when Oborcopy is "
                      "closed. It is installed as a systemd user service for your account (no root needed) "
                      "and starts automatically at boot.");
#endif
        break;
    default:
        if (!state.account.isEmpty())
            details << tr("Account: %1").arg(state.account);
        if (state.isRunning() && state.status.started.isValid())
            details << tr("Running since %1").arg(GuiUtil::formatWhen(state.status.started));
        if (!state.status.version.isEmpty() && state.isRunning())
            details << tr("Version %1").arg(state.status.version);
#ifndef Q_OS_WIN
        details << tr("systemd user unit “%1”").arg(ServiceInfo::unitName());
#endif
        break;
    }
    QString text = details.join(QStringLiteral("  ·  "));
    if (state.kind == ServiceState::Kind::Busy)
        text = tr("Backing up “%1” since %2.").arg(state.status.currentJob, GuiUtil::formatWhen(state.status.currentSince))
               + QStringLiteral("  ") + text;
    else if (state.kind == ServiceState::Kind::Running && !state.responding)
        text = tr("The service does not report back – check the service log.") + QStringLiteral("  ") + text;
    m_details->setText(text);

    // Problem banner: the last action result wins, then problems of the service itself.
    m_fix->setVisible(false);
    if (!m_actionMessage.isEmpty()) {
        showMessage(m_actionMessage, m_actionOk ? c.success : c.danger);
    } else if (state.binaryMismatch && state.isInstalled()) {
        showMessage(tr("The service uses another Oborcopy installation:\n%1").arg(state.installedBinary), c.warning);
        m_fix->setText(tr("Use this Oborcopy"));
        m_fix->setVisible(true);
    } else if (state.kind == ServiceState::Kind::Failed && !state.message.isEmpty()) {
        showMessage(state.message, c.danger);
        if (ServiceController::supportsPasswordUpdate()) {
            m_fix->setText(tr("Update password…"));
            m_fix->setVisible(true);
        }
    } else {
        m_message->setVisible(false);
    }

    const bool installed = state.isInstalled();
    const bool canInstall = QFileInfo::exists(ServiceController::serviceBinary());
    m_install->setVisible(!installed);
    m_install->setEnabled(!busy && canInstall && state.kind == ServiceState::Kind::NotInstalled);
    if (!canInstall)
        m_install->setToolTip(tr("%1 was not found next to Oborcopy.").arg(ServiceInfo::serviceExecutable()));
    for (QPushButton *b : {m_start, m_pause, m_stop, m_restart, m_uninstall})
        b->setVisible(installed);
    m_password->setVisible(installed && ServiceController::supportsPasswordUpdate());
    const bool stopped = state.kind == ServiceState::Kind::Stopped || state.kind == ServiceState::Kind::Failed;
    m_start->setEnabled(!busy && stopped);
    m_stop->setEnabled(!busy && (state.isRunning() || state.kind == ServiceState::Kind::Starting));
    m_restart->setEnabled(!busy && state.isRunning());
    m_pause->setEnabled(!busy && state.isRunning());
    const bool paused = state.kind == ServiceState::Kind::Paused;
    m_pause->setText(paused ? tr("Resume") : tr("Pause"));
    m_pause->setIcon(IconFactory::icon(paused ? IconKind::Play : IconKind::Pause));
    m_uninstall->setEnabled(!busy);
    m_password->setEnabled(!busy);

    switch (state.kind) {
    case ServiceState::Kind::NotInstalled:
        m_hint->setText(tr("Install the service (above) so these jobs run automatically."));
        break;
    case ServiceState::Kind::Stopped:
    case ServiceState::Kind::Failed:
        m_hint->setText(tr("The service is not running – scheduled jobs do not start."));
        break;
    case ServiceState::Kind::Paused:
        m_hint->setText(tr("Paused – no scheduled job starts until you resume. A job that is already running finishes."));
        break;
    default:
        m_hint->setText(tr("Changes are picked up by the service within a few seconds. Missed runs are "
                           "caught up after start-up if enabled for the job."));
        break;
    }
    refreshTable();
}

void ServiceDialog::showMessage(const QString &text, const QColor &color)
{
    QColor background = color;
    background.setAlphaF(0.12);
    m_message->setText(text);
    m_message->setStyleSheet(QStringLiteral("QLabel { border: 1px solid %1; border-radius: 8px; padding: 8px; "
                                            "background: rgba(%2,%3,%4,%5); }")
                                 .arg(color.name())
                                 .arg(background.red())
                                 .arg(background.green())
                                 .arg(background.blue())
                                 .arg(background.alphaF()));
    m_message->setVisible(true);
}

void ServiceDialog::onActionFinished(bool ok, const QString &message)
{
    m_actionOk = ok;
    m_actionMessage = message;
    updateState(m_controller->state());
}

void ServiceDialog::fixProblem()
{
    m_actionMessage.clear();
    if (m_controller->state().binaryMismatch)
        m_controller->repair();
    else
        m_controller->updatePassword();
}

void ServiceDialog::confirmUninstall()
{
    const auto answer = QMessageBox::question(
        this, tr("Uninstall service"),
        tr("Stop and remove the Oborcopy Scheduler service?\n\nYour schedule is kept and is used again if you "
           "install the service later."));
    if (answer == QMessageBox::Yes) {
        m_actionMessage.clear();
        m_controller->uninstall();
    }
}

// ---------------------------------------------------------------- schedule

void ServiceDialog::saveSchedule()
{
    QString error;
    if (!m_schedule.save(ServiceInfo::schedulePath(), &error))
        QMessageBox::warning(this, tr("Schedule"), tr("Could not save the schedule: %1").arg(error));
    refreshTable();
    emit scheduleChanged();
}

void ServiceDialog::refreshTable()
{
    m_runState = ServiceRunState::load(ServiceInfo::statePath());
    const QString selected = selectedIndex() >= 0 ? m_schedule.entries.at(selectedIndex()).job : QString();
    const QDateTime now = QDateTime::currentDateTime();
    const bool running = m_controller->state().isRunning();
    m_updatingTable = true;
    m_table->setRowCount(int(m_schedule.entries.size()));
    for (int row = 0; row < m_schedule.entries.size(); ++row) {
        const ScheduleEntry &entry = m_schedule.entries.at(row);
        const JobRunState run = m_runState.jobs.value(entry.job);
        const QDateTime anchor = run.anchor.isValid() && run.fingerprint == entry.fingerprint() ? run.anchor : now;
        const QDateTime next = entry.enabled ? entry.nextRun(anchor) : QDateTime();
        QString nextText = tr("Off");
        if (entry.enabled)
            nextText = !next.isValid() ? tr("Never") : (next <= now ? tr("due now") : GuiUtil::formatWhen(next));
        if (entry.enabled && !running && next.isValid())
            nextText += tr(" (service not running)");

        const QStringList cells = {QString(),
                                   entry.job,
                                   entry.describe(),
                                   nextText,
                                   run.lastRun.isValid() ? GuiUtil::formatWhen(run.lastRun) : tr("never"),
                                   run.lastStatus};
        for (int col = 0; col < ColumnCount; ++col) {
            QTableWidgetItem *item = m_table->item(row, col);
            if (!item) {
                item = new QTableWidgetItem;
                m_table->setItem(row, col, item);
            }
            item->setText(cells.at(col));
            item->setForeground(entry.enabled ? Theme::colors().text : Theme::colors().muted);
        }
        QTableWidgetItem *on = m_table->item(row, ColOn);
        on->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsUserCheckable);
        on->setCheckState(entry.enabled ? Qt::Checked : Qt::Unchecked);
        on->setToolTip(tr("Enable or disable this schedule"));
        m_table->item(row, ColJob)->setIcon(IconFactory::icon(IconKind::History));
        m_table->item(row, ColResult)->setForeground(statusColor(run.lastStatus));
        if (entry.job == selected)
            m_table->selectRow(row);
        if (!m_store.exists(entry.job)) {
            m_table->item(row, ColJob)->setToolTip(tr("This job no longer exists."));
            m_table->item(row, ColJob)->setForeground(Theme::colors().danger);
        }
    }
    m_updatingTable = false;
    if (selectedIndex() < 0 && m_table->rowCount() > 0 && selected.isEmpty())
        m_table->selectRow(0);
    updateSelectionButtons();
}

int ServiceDialog::selectedIndex() const
{
    const int row = m_table->currentRow();
    const auto selection = m_table->selectionModel()->selectedRows();
    if (selection.isEmpty() || row < 0 || row >= m_schedule.entries.size())
        return -1;
    return row;
}

void ServiceDialog::scheduleJob(const QString &job)
{
    const int index = m_schedule.indexOf(job);
    if (index >= 0) {
        m_table->selectRow(index);
        editEntryAt(index);
        return;
    }
    ScheduleEntry entry;
    entry.job = job;
    ScheduleEditorDialog dialog(entry, {job}, this);
    if (dialog.exec() != QDialog::Accepted)
        return;
    m_schedule.entries.append(dialog.entry());
    saveSchedule();
}

void ServiceDialog::addEntry()
{
    QStringList jobs;
    for (const QString &name : m_store.jobNames()) {
        if (!m_schedule.contains(name))
            jobs << name;
    }
    if (jobs.isEmpty()) {
        QMessageBox::information(this, tr("Schedule a job"),
                                 m_store.jobNames().isEmpty() ? tr("Create a job first.")
                                                              : tr("All jobs are already scheduled. Edit an "
                                                                   "existing entry to change its time."));
        return;
    }
    ScheduleEntry entry;
    entry.job = jobs.first();
    ScheduleEditorDialog dialog(entry, jobs, this);
    if (dialog.exec() != QDialog::Accepted)
        return;
    m_schedule.entries.append(dialog.entry());
    saveSchedule();
}

void ServiceDialog::editEntry()
{
    editEntryAt(selectedIndex());
}

void ServiceDialog::editEntryAt(int index)
{
    if (index < 0 || index >= m_schedule.entries.size())
        return;
    const ScheduleEntry current = m_schedule.entries.at(index);
    QStringList jobs{current.job};
    for (const QString &name : m_store.jobNames()) {
        if (!m_schedule.contains(name))
            jobs << name;
    }
    ScheduleEditorDialog dialog(current, jobs, this);
    if (dialog.exec() != QDialog::Accepted)
        return;
    m_schedule.entries[index] = dialog.entry();
    saveSchedule();
}

void ServiceDialog::removeEntry()
{
    const int index = selectedIndex();
    if (index < 0)
        return;
    const QString job = m_schedule.entries.at(index).job;
    if (QMessageBox::question(this, tr("Remove schedule"),
                              tr("Stop running “%1” automatically?\n\nThe job itself is not deleted.").arg(job))
        != QMessageBox::Yes)
        return;
    m_schedule.entries.remove(index);
    saveSchedule();
}

void ServiceDialog::onItemChanged(QTableWidgetItem *item)
{
    if (m_updatingTable || item->column() != ColOn || item->row() >= m_schedule.entries.size())
        return;
    m_schedule.entries[item->row()].enabled = item->checkState() == Qt::Checked;
    saveSchedule();
}

void ServiceDialog::showLastRun()
{
    const int index = selectedIndex();
    if (index < 0)
        return;
    QFile file(m_runState.jobs.value(m_schedule.entries.at(index).job).lastReport);
    if (!file.open(QIODevice::ReadOnly))
        return;
    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    if (!doc.isObject())
        return;
    SummaryDialog dialog(RunReport::fromJson(doc.object()), this);
    dialog.exec();
}

void ServiceDialog::showScheduledHistory()
{
    HistoryDialog dialog(HistoryDialog::logDirsFor(m_store), this);
    dialog.setFilter(HistoryDialog::Filter::Scheduled);
    dialog.exec();
}

} // namespace obor
