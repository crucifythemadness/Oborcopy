#include "AdvancedOptionsPage.h"

#include "AppPaths.h"
#include "CollapsibleSection.h"
#include "GuiUtil.h"
#include "IconFactory.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDir>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QSpinBox>
#include <QToolButton>
#include <QVBoxLayout>

namespace obor {

namespace {

constexpr double kMB = 1024.0 * 1024.0;

QSpinBox *spin(QWidget *parent, int min, int max, const QString &suffix,
               const QString &special = QString())
{
    auto *box = new QSpinBox(parent);
    box->setRange(min, max);
    box->setSuffix(suffix);
    box->setSpecialValueText(special);
    return box;
}

QDoubleSpinBox *doubleSpin(QWidget *parent, double max, const QString &suffix,
                           const QString &special = QString())
{
    auto *box = new QDoubleSpinBox(parent);
    box->setRange(0, max);
    box->setDecimals(1);
    box->setSuffix(suffix);
    box->setSpecialValueText(special);
    return box;
}

} // namespace

AdvancedOptionsPage::AdvancedOptionsPage(QWidget *parent)
    : QWidget(parent)
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 10, 0, 0);
    layout->setSpacing(10);
    layout->addWidget(buildCopySection());
    layout->addWidget(buildCompareSection());
    layout->addWidget(buildFilterSection());
    layout->addWidget(buildMetadataSection());
    layout->addWidget(buildRetrySection());
    layout->addWidget(buildSafetySection());
    layout->addWidget(buildLogSection());
    layout->addStretch();
    connectChangeSignals();
}

CollapsibleSection *AdvancedOptionsPage::section(const QString &title, const QString &subtitle,
                                                 QFormLayout *form)
{
    auto *s = new CollapsibleSection(title, subtitle, this);
    form->setHorizontalSpacing(16);
    form->setVerticalSpacing(8);
    s->setContentLayout(form);
    return s;
}

CollapsibleSection *AdvancedOptionsPage::buildCopySection()
{
    auto *form = new QFormLayout;
    m_copyEmptyDirs = new QCheckBox(tr("Copy empty folders"));
    m_maxDepth = spin(this, 0, 999, tr(" levels"), tr("Unlimited"));
    m_maxDepth->setToolTip(tr("1 = only the files directly in the source folder"));
    m_moveFiles = new QCheckBox(tr("Move files – delete them from the source after a successful copy"));
    m_skipOlder = new QCheckBox(tr("Never overwrite a newer destination file (skip older source files)"));
    m_skipNewer = new QCheckBox(tr("Skip files that are newer in the source"));
    m_overwriteAll = new QCheckBox(tr("Always copy every file, even unchanged ones"));
    m_parallel = spin(this, 1, 16, tr(" file(s) at once"));
    m_parallel->setToolTip(tr("More than 1 helps with SSDs and networks; HDDs are usually fastest with 1."));
    m_throughput = spin(this, 0, 100000, tr(" MB/s"), tr("Unlimited"));
    form->addRow(QString(), m_copyEmptyDirs);
    form->addRow(tr("Folder depth:"), m_maxDepth);
    form->addRow(QString(), m_skipOlder);
    form->addRow(QString(), m_skipNewer);
    form->addRow(QString(), m_overwriteAll);
    form->addRow(QString(), m_moveFiles);
    form->addRow(tr("Parallel copies:"), m_parallel);
    form->addRow(tr("Speed limit:"), m_throughput);
    return section(tr("Copy behaviour"), tr("Which files are copied and how."), form);
}

CollapsibleSection *AdvancedOptionsPage::buildCompareSection()
{
    auto *form = new QFormLayout;
    m_dst = new QCheckBox(tr("Ignore exact 1-hour differences (daylight saving time on FAT disks)"));
    m_checksum = new QCheckBox(tr("Compare file contents (SHA-256) – slow, reads every file"));
    m_verify = new QCheckBox(tr("Verify every copied file (re-read and compare checksums)"));
    form->addRow(QString(), m_dst);
    form->addRow(QString(), m_checksum);
    form->addRow(QString(), m_verify);
    return section(tr("Comparison & verification"),
                   tr("By default files are compared by size and modification time."), form);
}

CollapsibleSection *AdvancedOptionsPage::buildFilterSection()
{
    auto *form = new QFormLayout;
    m_includeFiles = new QLineEdit;
    m_includeFiles->setPlaceholderText(tr("all files – or e.g. *.mp3; *.flac"));
    m_minSize = doubleSpin(this, 1e7, tr(" MB"), tr("No limit"));
    m_maxSize = doubleSpin(this, 1e7, tr(" MB"), tr("No limit"));
    m_minAge = spin(this, 0, 100000, tr(" days"), tr("Off"));
    m_maxAge = spin(this, 0, 100000, tr(" days"), tr("Off"));
    m_skipHidden = new QCheckBox(tr("Skip hidden files and folders"));
    m_skipSystem = new QCheckBox(tr("Skip system files and folders (Windows)"));
    m_symlinks = new QComboBox;
    m_symlinks->addItems({tr("Skip symbolic links and junctions"), tr("Follow links (copy their targets)")});
    form->addRow(tr("Only files:"), m_includeFiles);
    form->addRow(tr("Minimum size:"), m_minSize);
    form->addRow(tr("Maximum size:"), m_maxSize);
    form->addRow(tr("Skip files newer than:"), m_minAge);
    form->addRow(tr("Skip files older than:"), m_maxAge);
    form->addRow(QString(), m_skipHidden);
    form->addRow(QString(), m_skipSystem);
    form->addRow(tr("Links:"), m_symlinks);
    return section(tr("Filters"),
                   tr("Excluded items are neither copied nor deleted in Mirror mode."), form);
}

CollapsibleSection *AdvancedOptionsPage::buildMetadataSection()
{
    auto *form = new QFormLayout;
    m_fileTimes = new QCheckBox(tr("Keep file modification times (needed for incremental runs)"));
    m_dirTimes = new QCheckBox(tr("Keep folder modification times"));
    m_attributes = new QCheckBox(tr("Copy file attributes (read-only, hidden, …)"));
    m_permissions = new QCheckBox(tr("Copy file permissions"));
#ifdef Q_OS_WIN
    m_permissions->setEnabled(false);
    m_permissions->setToolTip(tr("Only used on Linux"));
#else
    m_attributes->setEnabled(false);
    m_attributes->setToolTip(tr("Only used on Windows"));
#endif
    form->addRow(QString(), m_fileTimes);
    form->addRow(QString(), m_dirTimes);
    form->addRow(QString(), m_attributes);
    form->addRow(QString(), m_permissions);
    return section(tr("Timestamps & attributes"), tr("What is copied besides the file data."), form);
}

CollapsibleSection *AdvancedOptionsPage::buildRetrySection()
{
    auto *form = new QFormLayout;
    m_baseWait = doubleSpin(this, 600, tr(" s"));
    m_maxWait = doubleSpin(this, 3600, tr(" s"));
    form->addRow(tr("First wait:"), m_baseWait);
    form->addRow(tr("Longest wait:"), m_maxWait);
    return section(tr("Retry timing"),
                   tr("The wait doubles with every retry (with a little random jitter) up to the "
                      "longest wait. The number of retries is set on the Basic tab."), form);
}

CollapsibleSection *AdvancedOptionsPage::buildSafetySection()
{
    auto *form = new QFormLayout;
    m_deleteMode = new QComboBox;
    m_deleteMode->addItems({tr("Delete permanently"),
                            tr("Move to archive folder \".oborcopy-archive\" in the destination")});
    m_thresholdEnabled = new QCheckBox(tr("Warn when more than"));
    m_thresholdPercent = spin(this, 1, 100, tr(" % of the destination files would be deleted"));
    auto *threshold = new QHBoxLayout;
    threshold->addWidget(m_thresholdEnabled);
    threshold->addWidget(m_thresholdPercent, 1);
    m_refuseEmpty = new QCheckBox(tr("Stop if the source folder is empty (e.g. disk not connected)"));
    form->addRow(tr("Deleted files:"), m_deleteMode);
    form->addRow(QString(), threshold);
    form->addRow(QString(), m_refuseEmpty);
    return section(tr("Mirror safety"),
                   tr("Protection against deleting your backup by picking the wrong folder."), form);
}

CollapsibleSection *AdvancedOptionsPage::buildLogSection()
{
    auto *form = new QFormLayout;
    m_logDir = new QLineEdit;
    m_logDir->setPlaceholderText(QDir::toNativeSeparators(AppPaths::defaultLogDir()));
    auto *browse = new QToolButton;
    browse->setIcon(IconFactory::icon(IconKind::Folder));
    auto *dirRow = new QHBoxLayout;
    dirRow->addWidget(m_logDir, 1);
    dirRow->addWidget(browse);
    m_logLevel = new QComboBox;
    m_logLevel->addItems({tr("Summary only"), tr("Changes (recommended)"), tr("All files")});
    m_keepLogs = spin(this, 1, 10000, tr(" runs"));
    form->addRow(tr("Log folder:"), dirRow);
    form->addRow(tr("Detail:"), m_logLevel);
    form->addRow(tr("Keep logs of the last:"), m_keepLogs);
    connect(browse, &QToolButton::clicked, this, [this]() {
        const QString dir = GuiUtil::chooseDirectory(this, tr("Log folder"), m_logDir->text());
        if (!dir.isEmpty()) {
            m_logDir->setText(QDir::toNativeSeparators(dir));
            emit changed();
        }
    });
    return section(tr("Logging"), tr("Every run writes a log file and a detailed report."), form);
}

void AdvancedOptionsPage::connectChangeSignals()
{
    for (auto *w : findChildren<QCheckBox *>())
        connect(w, &QCheckBox::toggled, this, &AdvancedOptionsPage::changed);
    for (auto *w : findChildren<QSpinBox *>())
        connect(w, &QSpinBox::valueChanged, this, &AdvancedOptionsPage::changed);
    for (auto *w : findChildren<QDoubleSpinBox *>())
        connect(w, &QDoubleSpinBox::valueChanged, this, &AdvancedOptionsPage::changed);
    for (auto *w : findChildren<QComboBox *>())
        connect(w, &QComboBox::currentIndexChanged, this, &AdvancedOptionsPage::changed);
    for (auto *w : findChildren<QLineEdit *>()) {
        if (!qobject_cast<QAbstractSpinBox *>(w->parentWidget()))
            connect(w, &QLineEdit::textEdited, this, &AdvancedOptionsPage::changed);
    }
}

void AdvancedOptionsPage::load(const JobOptions &job)
{
    const QSignalBlocker blocker(this);
    loadCopy(job.copy);
    loadFilter(job.filter);
    loadOther(job);
}

void AdvancedOptionsPage::store(JobOptions &job) const
{
    storeCopy(job.copy);
    storeFilter(job.filter);
    storeOther(job);
}

void AdvancedOptionsPage::loadCopy(const CopyOptions &c)
{
    m_copyEmptyDirs->setChecked(c.copyEmptyDirs);
    m_maxDepth->setValue(c.maxDepth);
    m_moveFiles->setChecked(c.moveFiles);
    m_skipOlder->setChecked(c.skipOlder);
    m_skipNewer->setChecked(c.skipNewer);
    m_overwriteAll->setChecked(c.overwriteAll);
    m_parallel->setValue(c.parallelCopies);
    m_throughput->setValue(c.throughputLimitMBps);
    m_checksum->setChecked(c.compareChecksum);
    m_verify->setChecked(c.verifyAfterCopy);
}

void AdvancedOptionsPage::storeCopy(CopyOptions &c) const
{
    c.copyEmptyDirs = m_copyEmptyDirs->isChecked();
    c.maxDepth = m_maxDepth->value();
    c.moveFiles = m_moveFiles->isChecked();
    c.skipOlder = m_skipOlder->isChecked();
    c.skipNewer = m_skipNewer->isChecked();
    c.overwriteAll = m_overwriteAll->isChecked();
    c.parallelCopies = m_parallel->value();
    c.throughputLimitMBps = m_throughput->value();
    c.compareChecksum = m_checksum->isChecked();
    c.verifyAfterCopy = m_verify->isChecked();
}

void AdvancedOptionsPage::loadFilter(const FilterOptions &f)
{
    m_includeFiles->setText(GuiUtil::joinPatterns(f.includeFiles));
    m_minSize->setValue(double(f.minSize) / kMB);
    m_maxSize->setValue(double(f.maxSize) / kMB);
    m_minAge->setValue(f.minAgeDays);
    m_maxAge->setValue(f.maxAgeDays);
    m_skipHidden->setChecked(f.skipHidden);
    m_skipSystem->setChecked(f.skipSystem);
    m_symlinks->setCurrentIndex(f.symlinks == SymlinkMode::Follow ? 1 : 0);
}

void AdvancedOptionsPage::storeFilter(FilterOptions &f) const
{
    f.includeFiles = GuiUtil::splitPatterns(m_includeFiles->text());
    f.minSize = qint64(m_minSize->value() * kMB);
    f.maxSize = qint64(m_maxSize->value() * kMB);
    f.minAgeDays = m_minAge->value();
    f.maxAgeDays = m_maxAge->value();
    f.skipHidden = m_skipHidden->isChecked();
    f.skipSystem = m_skipSystem->isChecked();
    f.symlinks = m_symlinks->currentIndex() == 1 ? SymlinkMode::Follow : SymlinkMode::Skip;
}

void AdvancedOptionsPage::loadOther(const JobOptions &job)
{
    m_dst->setChecked(job.compare.dstTolerance);
    m_fileTimes->setChecked(job.metadata.fileTimes);
    m_dirTimes->setChecked(job.metadata.dirTimes);
    m_attributes->setChecked(job.metadata.attributes);
    m_permissions->setChecked(job.metadata.permissions);
    m_baseWait->setValue(job.retry.baseWaitMs / 1000.0);
    m_maxWait->setValue(job.retry.maxWaitMs / 1000.0);
    m_deleteMode->setCurrentIndex(job.safety.deleteMode == DeleteMode::Archive ? 1 : 0);
    m_thresholdEnabled->setChecked(job.safety.thresholdEnabled);
    m_thresholdPercent->setValue(job.safety.thresholdPercent);
    m_refuseEmpty->setChecked(job.safety.refuseEmptySource);
    m_logDir->setText(QDir::toNativeSeparators(job.log.logDir));
    m_logLevel->setCurrentIndex(int(job.log.level));
    m_keepLogs->setValue(job.log.keepLogs);
}

void AdvancedOptionsPage::storeOther(JobOptions &job) const
{
    job.compare.dstTolerance = m_dst->isChecked();
    job.metadata.fileTimes = m_fileTimes->isChecked();
    job.metadata.dirTimes = m_dirTimes->isChecked();
    job.metadata.attributes = m_attributes->isChecked();
    job.metadata.permissions = m_permissions->isChecked();
    job.retry.baseWaitMs = int(m_baseWait->value() * 1000);
    job.retry.maxWaitMs = int(m_maxWait->value() * 1000);
    job.safety.deleteMode = m_deleteMode->currentIndex() == 1 ? DeleteMode::Archive
                                                              : DeleteMode::Permanent;
    job.safety.thresholdEnabled = m_thresholdEnabled->isChecked();
    job.safety.thresholdPercent = m_thresholdPercent->value();
    job.safety.refuseEmptySource = m_refuseEmpty->isChecked();
    job.log.logDir = QDir::fromNativeSeparators(m_logDir->text().trimmed());
    job.log.level = LogLevel(m_logLevel->currentIndex());
    job.log.keepLogs = m_keepLogs->value();
}

} // namespace obor
