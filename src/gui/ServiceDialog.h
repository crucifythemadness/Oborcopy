#pragma once

#include "Schedule.h"
#include "ServiceController.h"
#include "ServiceFiles.h"

#include <QDialog>

class QLabel;
class QPushButton;
class QTableWidget;
class QTableWidgetItem;

namespace obor {

class JobStore;

// The Service Manager: installs and controls the scheduler service and edits
// which jobs it runs when. Changes to the schedule are saved immediately; the
// running service picks them up within a few seconds.
class ServiceDialog : public QDialog
{
    Q_OBJECT

public:
    ServiceDialog(ServiceController *controller, JobStore &store, QWidget *parent = nullptr);

    // Opens the schedule editor for this job (new entry if not scheduled yet).
    void scheduleJob(const QString &job);

signals:
    void scheduleChanged();

private:
    QWidget *buildStatusCard();
    QWidget *buildScheduleCard();
    void updateState(const ServiceState &state);
    void onActionFinished(bool ok, const QString &message);
    void showMessage(const QString &text, const QColor &color);
    void fixProblem();

    void saveSchedule();
    void refreshTable();
    int selectedIndex() const;
    void updateSelectionButtons();
    void addEntry();
    void editEntry();
    void editEntryAt(int index);
    void removeEntry();
    void onItemChanged(QTableWidgetItem *item);
    void showLastRun();
    void showScheduledHistory();
    void confirmUninstall();

    ServiceController *m_controller;
    JobStore &m_store;
    ServiceSchedule m_schedule;
    ServiceRunState m_runState;
    bool m_updatingTable = false;
    QString m_actionMessage;
    bool m_actionOk = true;

    QLabel *m_dot = nullptr;
    QLabel *m_title = nullptr;
    QLabel *m_details = nullptr;
    QLabel *m_message = nullptr;
    QPushButton *m_fix = nullptr;
    QPushButton *m_install = nullptr;
    QPushButton *m_uninstall = nullptr;
    QPushButton *m_start = nullptr;
    QPushButton *m_pause = nullptr;
    QPushButton *m_stop = nullptr;
    QPushButton *m_restart = nullptr;
    QPushButton *m_password = nullptr;
    QTableWidget *m_table = nullptr;
    QPushButton *m_edit = nullptr;
    QPushButton *m_remove = nullptr;
    QPushButton *m_lastRun = nullptr;
    QLabel *m_hint = nullptr;
};

} // namespace obor
