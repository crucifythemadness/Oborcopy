#include "BasicOptionsPage.h"

#include "GuiUtil.h"
#include "ModeCard.h"

#include <QCheckBox>
#include <QComboBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QSpinBox>
#include <QVBoxLayout>

namespace obor {

BasicOptionsPage::BasicOptionsPage(QWidget *parent)
    : QWidget(parent)
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 10, 0, 0);
    layout->setSpacing(12);
    layout->addWidget(buildModeCards());
    layout->addWidget(buildOptions());
    layout->addStretch();
}

QWidget *BasicOptionsPage::buildModeCards()
{
    m_updateCard = new ModeCard(IconKind::UpdateMode, tr("Update"),
                                tr("Copy new and changed files. Nothing is ever deleted in "
                                   "the destination."), this);
    m_mirrorCard = new ModeCard(IconKind::MirrorMode, tr("Mirror"),
                                tr("Make the destination an exact copy of the source. Files "
                                   "that no longer exist in the source are deleted. A preview "
                                   "is shown before anything is deleted."), this);
    m_mirrorCard->setBadge(tr("deletes"), QColor(0xD6, 0x45, 0x45));

    auto *row = new QWidget(this);
    auto *layout = new QHBoxLayout(row);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(12);
    layout->addWidget(m_updateCard, 1);
    layout->addWidget(m_mirrorCard, 1);

    connect(m_updateCard, &ModeCard::clicked, this, [this]() { setMode(SyncMode::Update); });
    connect(m_mirrorCard, &ModeCard::clicked, this, [this]() { setMode(SyncMode::Mirror); });
    return row;
}

QWidget *BasicOptionsPage::buildOptions()
{
    auto *card = GuiUtil::makeCard(this);
    m_recursive = new QCheckBox(tr("Include subfolders"), card);
    m_retries = new QSpinBox(card);
    m_retries->setRange(0, 100);
    m_retries->setSuffix(tr(" retries"));
    m_retries->setMaximumWidth(140);
    m_retries->setToolTip(tr("How often a locked or unreadable file is retried. The wait grows "
                             "with each attempt (2 s, 4 s, 8 s, …)."));
    m_tolerance = new QComboBox(card);
    m_tolerance->addItem(tr("Automatic (2 s on FAT/exFAT, otherwise exact)"));
    m_tolerance->addItem(tr("Custom"));
    m_toleranceMs = new QSpinBox(card);
    m_toleranceMs->setRange(0, 3600000);
    m_toleranceMs->setSingleStep(1000);
    m_toleranceMs->setSuffix(tr(" ms"));
    m_toleranceMs->setMinimumWidth(110);
    m_excludeFiles = new QLineEdit(card);
    m_excludeFiles->setPlaceholderText(tr("e.g. *.tmp; Thumbs.db; desktop.ini"));
    m_excludeDirs = new QLineEdit(card);
    m_excludeDirs->setPlaceholderText(tr("e.g. $RECYCLE.BIN; System Volume Information"));
    m_dryRun = new QCheckBox(tr("Preview only (dry run) – show what would happen, change nothing"), card);

    auto *tolerance = new QHBoxLayout;
    tolerance->addWidget(m_tolerance, 1);
    tolerance->addWidget(m_toleranceMs);

    auto *form = new QFormLayout(card);
    form->setContentsMargins(16, 14, 16, 14);
    form->setHorizontalSpacing(16);
    form->setVerticalSpacing(10);
    form->addRow(GuiUtil::makeLabel(tr("Options"), QStringLiteral("SectionTitle")));
    form->addRow(QString(), m_recursive);
    form->addRow(tr("Locked files:"), m_retries);
    form->addRow(tr("Time tolerance:"), tolerance);
    form->addRow(tr("Exclude files:"), m_excludeFiles);
    form->addRow(tr("Exclude folders:"), m_excludeDirs);
    form->addRow(QString(), m_dryRun);

    for (QCheckBox *box : {m_recursive, m_dryRun})
        connect(box, &QCheckBox::toggled, this, &BasicOptionsPage::changed);
    for (QSpinBox *spin : {m_retries, m_toleranceMs})
        connect(spin, &QSpinBox::valueChanged, this, &BasicOptionsPage::changed);
    for (QLineEdit *edit : {m_excludeFiles, m_excludeDirs})
        connect(edit, &QLineEdit::textEdited, this, &BasicOptionsPage::changed);
    connect(m_tolerance, &QComboBox::currentIndexChanged, this, [this]() {
        updateToleranceState();
        emit changed();
    });
    return card;
}

void BasicOptionsPage::setMode(SyncMode mode)
{
    m_mode = mode;
    m_updateCard->setChecked(mode == SyncMode::Update);
    m_mirrorCard->setChecked(mode == SyncMode::Mirror);
    emit changed();
}

void BasicOptionsPage::updateToleranceState()
{
    m_toleranceMs->setEnabled(m_tolerance->currentIndex() == 1);
}

void BasicOptionsPage::load(const JobOptions &job)
{
    const QSignalBlocker b1(m_recursive), b2(m_retries), b3(m_tolerance), b4(m_toleranceMs),
        b5(m_dryRun);
    m_mode = job.mode;
    m_updateCard->setChecked(job.mode == SyncMode::Update);
    m_mirrorCard->setChecked(job.mode == SyncMode::Mirror);
    m_recursive->setChecked(job.copy.recursive);
    m_retries->setValue(job.retry.retries);
    m_tolerance->setCurrentIndex(job.compare.toleranceMode == ToleranceMode::Auto ? 0 : 1);
    m_toleranceMs->setValue(job.compare.toleranceMs);
    m_excludeFiles->setText(GuiUtil::joinPatterns(job.filter.excludeFiles));
    m_excludeDirs->setText(GuiUtil::joinPatterns(job.filter.excludeDirs));
    m_dryRun->setChecked(job.copy.dryRun);
    updateToleranceState();
}

void BasicOptionsPage::store(JobOptions &job) const
{
    job.mode = m_mode;
    job.copy.recursive = m_recursive->isChecked();
    job.retry.retries = m_retries->value();
    job.compare.toleranceMode = m_tolerance->currentIndex() == 0 ? ToleranceMode::Auto
                                                                 : ToleranceMode::Custom;
    job.compare.toleranceMs = m_toleranceMs->value();
    job.filter.excludeFiles = GuiUtil::splitPatterns(m_excludeFiles->text());
    job.filter.excludeDirs = GuiUtil::splitPatterns(m_excludeDirs->text());
    job.copy.dryRun = m_dryRun->isChecked();
}

} // namespace obor
