#include "ScheduleEditorDialog.h"

#include "GuiUtil.h"

#include <QButtonGroup>
#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLocale>
#include <QPushButton>
#include <QSpinBox>
#include <QStackedWidget>
#include <QTimeEdit>
#include <QToolButton>
#include <QVBoxLayout>

namespace obor {

namespace {

constexpr int kPreviewRuns = 4;

QToolButton *chip(const QString &text, QWidget *parent)
{
    auto *button = new QToolButton(parent);
    button->setObjectName(QStringLiteral("Chip"));
    button->setText(text);
    button->setCheckable(true);
    button->setCursor(Qt::PointingHandCursor);
    return button;
}

} // namespace

ScheduleEditorDialog::ScheduleEditorDialog(const ScheduleEntry &entry, const QStringList &jobs, QWidget *parent)
    : QDialog(parent)
    , m_entry(entry)
    , m_job(new QComboBox(this))
    , m_frequency(new QButtonGroup(this))
    , m_details(new QStackedWidget(this))
    , m_interval(new QSpinBox(this))
    , m_minute(new QSpinBox(this))
    , m_dayOfMonth(new QSpinBox(this))
    , m_time(new QTimeEdit(this))
    , m_enabled(new QCheckBox(tr("Enabled"), this))
    , m_catchUp(new QCheckBox(tr("Run a missed backup once after start-up\n(e.g. when the computer was off at the scheduled time)"), this))
    , m_allowDeletions(new QCheckBox(tr("Allow deletions in Mirror mode\n(the job's deletion safety threshold still applies)"), this))
    , m_preview(GuiUtil::makeLabel(QString(), QStringLiteral("Muted"), this))
{
    setWindowTitle(entry.job.isEmpty() ? tr("Schedule a job") : tr("Edit schedule"));
    setMinimumWidth(520);

    m_job->addItems(jobs);
    m_job->setCurrentText(entry.job);

    // Frequency as a segmented row of chips.
    auto *frequencyRow = new QHBoxLayout;
    frequencyRow->setSpacing(6);
    const std::pair<Frequency, QString> frequencies[] = {{Frequency::Hourly, tr("Every few hours")},
                                                         {Frequency::Daily, tr("Daily")},
                                                         {Frequency::Weekly, tr("Weekly")},
                                                         {Frequency::Monthly, tr("Monthly")}};
    for (const auto &[frequency, label] : frequencies) {
        QToolButton *button = chip(label, this);
        m_frequency->addButton(button, int(frequency));
        frequencyRow->addWidget(button);
    }
    frequencyRow->addStretch();
    m_frequency->setExclusive(true);
    m_frequency->button(int(entry.frequency))->setChecked(true);

    // Hourly: every [n] hours at minute [m]
    auto *hourly = new QWidget;
    auto *hourlyRow = new QHBoxLayout(hourly);
    hourlyRow->setContentsMargins(0, 0, 0, 0);
    m_interval->setRange(1, 23);
    m_interval->setValue(entry.intervalHours);
    m_minute->setRange(0, 59);
    m_minute->setValue(entry.time.minute());
    m_minute->setPrefix(QStringLiteral(":"));
    hourlyRow->addWidget(new QLabel(tr("Every"), hourly));
    hourlyRow->addWidget(m_interval);
    hourlyRow->addWidget(new QLabel(tr("hour(s), at minute"), hourly));
    hourlyRow->addWidget(m_minute);
    hourlyRow->addStretch();

    // Daily: nothing extra (time row only)
    auto *daily = new QWidget;

    // Weekly: weekday chips
    auto *weekly = new QWidget;
    auto *weeklyRow = new QHBoxLayout(weekly);
    weeklyRow->setContentsMargins(0, 0, 0, 0);
    weeklyRow->setSpacing(4);
    for (int day = 1; day <= 7; ++day) {
        QToolButton *button = chip(QLocale().dayName(day, QLocale::ShortFormat), weekly);
        button->setChecked(entry.weekdays.contains(day));
        connect(button, &QToolButton::toggled, this, &ScheduleEditorDialog::updatePreview);
        m_days << button;
        weeklyRow->addWidget(button);
    }
    weeklyRow->addStretch();

    // Monthly: day of month (0 = last day)
    auto *monthly = new QWidget;
    auto *monthlyRow = new QHBoxLayout(monthly);
    monthlyRow->setContentsMargins(0, 0, 0, 0);
    m_dayOfMonth->setRange(0, 31);
    m_dayOfMonth->setSpecialValueText(tr("Last day"));
    m_dayOfMonth->setValue(entry.dayOfMonth);
    m_dayOfMonth->setToolTip(tr("Days that a month does not have (e.g. 31) run on its last day."));
    monthlyRow->addWidget(new QLabel(tr("On day"), monthly));
    monthlyRow->addWidget(m_dayOfMonth);
    monthlyRow->addWidget(new QLabel(tr("of the month"), monthly));
    monthlyRow->addStretch();

    m_details->addWidget(hourly);
    m_details->addWidget(daily);
    m_details->addWidget(weekly);
    m_details->addWidget(monthly);

    m_time->setDisplayFormat(QStringLiteral("HH:mm"));
    m_time->setTime(entry.time);
    m_enabled->setChecked(entry.enabled);
    m_catchUp->setChecked(entry.catchUp);
    m_allowDeletions->setChecked(entry.allowDeletions);
    m_preview->setWordWrap(true);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    buttons->button(QDialogButtonBox::Ok)->setObjectName(QStringLiteral("Primary"));
    buttons->button(QDialogButtonBox::Ok)->setText(tr("Save schedule"));
    m_ok = buttons->button(QDialogButtonBox::Ok);
    m_hasJobs = !jobs.isEmpty();

    auto *form = new QFormLayout;
    form->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);
    form->setVerticalSpacing(12);
    form->addRow(tr("Job:"), m_job);
    form->addRow(tr("Repeat:"), frequencyRow);
    form->addRow(QString(), m_details);
    form->addRow(tr("Time:"), buildTimingRow());
    form->addRow(QString(), m_enabled);
    form->addRow(QString(), m_catchUp);
    form->addRow(QString(), m_allowDeletions);

    auto *layout = new QVBoxLayout(this);
    layout->addWidget(GuiUtil::makeLabel(windowTitle(), QStringLiteral("Title"), this));
    layout->addLayout(form);
    layout->addSpacing(4);
    layout->addWidget(m_preview);
    layout->addStretch();
    layout->addWidget(buttons);

    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(m_frequency, &QButtonGroup::idClicked, this, &ScheduleEditorDialog::updateVisibility);
    connect(m_interval, &QSpinBox::valueChanged, this, &ScheduleEditorDialog::updatePreview);
    connect(m_minute, &QSpinBox::valueChanged, this, &ScheduleEditorDialog::updatePreview);
    connect(m_dayOfMonth, &QSpinBox::valueChanged, this, &ScheduleEditorDialog::updatePreview);
    connect(m_time, &QTimeEdit::timeChanged, this, &ScheduleEditorDialog::updatePreview);
    connect(m_enabled, &QCheckBox::toggled, this, &ScheduleEditorDialog::updatePreview);
    updateVisibility();
}

QWidget *ScheduleEditorDialog::buildTimingRow()
{
    m_timeRow = new QWidget(this);
    auto *row = new QHBoxLayout(m_timeRow);
    row->setContentsMargins(0, 0, 0, 0);
    row->addWidget(m_time);
    row->addStretch();
    return m_timeRow;
}

void ScheduleEditorDialog::updateVisibility()
{
    const int frequency = m_frequency->checkedId();
    m_details->setCurrentIndex(frequency);
    m_details->setVisible(frequency != int(Frequency::Daily));
    m_time->setEnabled(frequency != int(Frequency::Hourly));
    m_timeRow->setToolTip(frequency == int(Frequency::Hourly) ? tr("Set the minute above.") : QString());
    updatePreview();
}

ScheduleEntry ScheduleEditorDialog::entry() const
{
    ScheduleEntry e = m_entry;
    e.job = m_job->currentText();
    e.frequency = Frequency(m_frequency->checkedId());
    e.intervalHours = m_interval->value();
    e.time = e.frequency == Frequency::Hourly ? QTime(0, m_minute->value()) : m_time->time();
    e.weekdays.clear();
    for (int i = 0; i < m_days.size(); ++i) {
        if (m_days.at(i)->isChecked())
            e.weekdays << i + 1;
    }
    e.dayOfMonth = m_dayOfMonth->value();
    e.enabled = m_enabled->isChecked();
    e.catchUp = m_catchUp->isChecked();
    e.allowDeletions = m_allowDeletions->isChecked();
    return e;
}

void ScheduleEditorDialog::updatePreview()
{
    const ScheduleEntry e = entry();
    QStringList runs;
    QDateTime t = QDateTime::currentDateTime();
    for (int i = 0; i < kPreviewRuns; ++i) {
        t = e.nextRun(t);
        if (!t.isValid())
            break;
        runs << GuiUtil::formatWhen(t);
    }
    m_ok->setEnabled(m_hasJobs && !runs.isEmpty());
    if (runs.isEmpty())
        m_preview->setText(tr("⚠ This schedule never runs – choose at least one weekday."));
    else
        m_preview->setText(tr("<b>%1</b><br>Next runs: %2%3")
                               .arg(e.describe().toHtmlEscaped(), runs.join(QStringLiteral(" · ")),
                                    e.enabled ? QString() : tr(" <i>(disabled)</i>")));
}

} // namespace obor
