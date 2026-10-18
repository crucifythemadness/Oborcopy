#pragma once

#include "Schedule.h"

#include <QDialog>

class QButtonGroup;
class QCheckBox;
class QComboBox;
class QLabel;
class QPushButton;
class QSpinBox;
class QStackedWidget;
class QTimeEdit;
class QToolButton;

namespace obor {

// Edits when the scheduler service runs one job: every n hours, daily,
// weekly on chosen days or monthly, plus catch-up and deletion options.
// Shows the next few run times as a live preview.
class ScheduleEditorDialog : public QDialog
{
    Q_OBJECT

public:
    // 'jobs' are the jobs that can be chosen (the entry's own job included).
    ScheduleEditorDialog(const ScheduleEntry &entry, const QStringList &jobs, QWidget *parent = nullptr);

    ScheduleEntry entry() const;

private:
    QWidget *buildTimingRow();
    void updateVisibility();
    void updatePreview();

    ScheduleEntry m_entry;
    QComboBox *m_job;
    QButtonGroup *m_frequency;
    QStackedWidget *m_details;
    QSpinBox *m_interval;
    QSpinBox *m_minute;
    QList<QToolButton *> m_days;
    QSpinBox *m_dayOfMonth;
    QWidget *m_timeRow;
    QTimeEdit *m_time;
    QCheckBox *m_enabled;
    QCheckBox *m_catchUp;
    QCheckBox *m_allowDeletions;
    QCheckBox *m_retry;
    QWidget *m_retryRow = nullptr;
    QSpinBox *m_retryCount;
    QSpinBox *m_retryInterval;
    QLabel *m_preview;
    QPushButton *m_ok = nullptr;
    bool m_hasJobs = false;
};

} // namespace obor
