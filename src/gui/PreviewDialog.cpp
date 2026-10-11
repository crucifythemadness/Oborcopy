#include "PreviewDialog.h"

#include "GuiUtil.h"
#include "IconFactory.h"
#include "Theme.h"

#include <QCheckBox>
#include <QDir>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

namespace obor {

PreviewDialog::PreviewDialog(const SyncPlan &plan, const JobOptions &job, bool previewMode,
                             QWidget *parent)
    : QDialog(parent)
    , m_plan(plan)
    , m_job(job)
{
    setWindowTitle(previewMode ? tr("Preview – %1").arg(job.name)
                               : tr("Confirm sync – %1").arg(job.name));
    resize(980, 680);

    auto *title = GuiUtil::makeLabel(previewMode ? tr("This is what the sync would do")
                                                 : tr("Please review the changes"),
                                     QStringLiteral("Title"), this);
    auto *paths = GuiUtil::makeLabel(QStringLiteral("%1  →  %2  (%3)")
                                         .arg(GuiUtil::displayPath(job.source),
                                              GuiUtil::displayPath(job.destination),
                                              toString(job.mode)),
                                     QStringLiteral("Muted"), this);
    paths->setWordWrap(true);
    auto *tree = new ResultTreeView(this);
    tree->setRows(rowsFromPlan(plan), true);

    auto *layout = new QVBoxLayout(this);
    layout->setSpacing(10);
    layout->addWidget(title);
    layout->addWidget(paths);
    layout->addWidget(buildTiles());
    addWarnings(layout);
    layout->addWidget(tree, 1);
    layout->addWidget(buildButtons());
    updateStartButton();
}

QVector<ResultRow> PreviewDialog::rowsFromPlan(const SyncPlan &plan)
{
    QVector<ResultRow> rows;
    rows.reserve(plan.deletions.size() + plan.createDirs.size() + plan.copies.size());
    for (const SyncAction &a : plan.deletions)
        rows.append({a.relPath, a.type == ActionType::DeleteDir ? ItemKind::DirDeleted : ItemKind::Deleted,
                     a.size, a.reason, {}, 0});
    for (const SyncAction &a : plan.createDirs)
        rows.append({a.relPath, ItemKind::DirCreated, 0, a.reason, {}, 0});
    for (const SyncAction &a : plan.copies)
        rows.append({a.relPath, a.type == ActionType::CopyNew ? ItemKind::Copied : ItemKind::Updated,
                     a.size, a.reason, {}, 0});
    for (const ScanError &e : plan.scanErrors)
        rows.append({e.relPath, ItemKind::Error, 0, e.message, tr("Scan"), 0});
    return rows;
}

QWidget *PreviewDialog::buildTiles()
{
    const ThemeColors &c = Theme::colors();
    auto *tiles = new QWidget(this);
    auto *row = new QHBoxLayout(tiles);
    row->setContentsMargins(0, 0, 0, 0);
    row->addWidget(GuiUtil::makeStatTile(GuiUtil::formatCount(m_plan.filesChecked), tr("files checked"), c.text));
    row->addWidget(GuiUtil::makeStatTile(GuiUtil::formatCount(m_plan.count(ActionType::CopyNew)), tr("new"), c.success));
    row->addWidget(GuiUtil::makeStatTile(GuiUtil::formatCount(m_plan.count(ActionType::Update)), tr("to update"), c.update));
    row->addWidget(GuiUtil::makeStatTile(GuiUtil::formatCount(m_plan.count(ActionType::DeleteFile)), tr("to delete"), c.danger));
    row->addWidget(GuiUtil::makeStatTile(GuiUtil::formatBytes(m_plan.bytesToCopy), tr("to copy"), c.accent));
    return tiles;
}

void PreviewDialog::addWarnings(QVBoxLayout *layout)
{
    const ThemeColors &c = Theme::colors();
    const int files = m_plan.count(ActionType::DeleteFile);
    const int dirs = m_plan.count(ActionType::DeleteDir);
    if (m_plan.thresholdExceeded) {
        layout->addWidget(GuiUtil::makeBanner(IconFactory::icon(IconKind::Error, c.danger),
            tr("<b>%1 % of the files in the destination would be deleted.</b> This is more than "
               "the configured limit of %2 %. Make sure the source folder is the right one and "
               "is complete (is the disk connected?).")
                .arg(m_plan.deletionPercent, 0, 'f', 1).arg(m_job.safety.thresholdPercent), c.danger, this));
    }
    if (files + dirs > 0) {
        const QString how = m_job.safety.deleteMode == DeleteMode::Archive
                                ? tr("moved to the archive folder in the destination")
                                : tr("<b>permanently deleted</b>");
        layout->addWidget(GuiUtil::makeBanner(IconFactory::icon(IconKind::Warning, c.warning),
            tr("%1 file(s) and %2 folder(s) that no longer exist in the source will be %3 in %4.")
                .arg(files).arg(dirs).arg(how, GuiUtil::displayPath(m_job.destination).toHtmlEscaped()),
            c.warning, this));
    }
    if (m_plan.spaceWarning()) {
        layout->addWidget(GuiUtil::makeBanner(IconFactory::icon(IconKind::Warning, c.warning),
            tr("The destination has only %1 free, but up to %2 will be copied.")
                .arg(GuiUtil::formatBytes(m_plan.destBytesFree), GuiUtil::formatBytes(m_plan.bytesToCopy)),
            c.warning, this));
    }
    if (!m_plan.hasWork()) {
        layout->addWidget(GuiUtil::makeBanner(IconFactory::icon(IconKind::Check, c.success),
            tr("<b>Everything is up to date.</b> There is nothing to copy or delete."), c.success, this));
    }
}

QWidget *PreviewDialog::buildButtons()
{
    auto *bar = new QWidget(this);
    auto *row = new QHBoxLayout(bar);
    row->setContentsMargins(0, 0, 0, 0);
    const int deletions = int(m_plan.deletions.size());
    if (deletions > 0) {
        m_confirm = new QCheckBox(tr("I have &checked the list – delete %1 item(s) in the destination")
                                      .arg(deletions), bar);
        connect(m_confirm, &QCheckBox::toggled, this, &PreviewDialog::updateStartButton);
        row->addWidget(m_confirm);
    }
    row->addStretch();
    auto *cancel = new QPushButton(m_plan.hasWork() ? tr("Cancel") : tr("Close"), bar);
    connect(cancel, &QPushButton::clicked, this, &QDialog::reject);
    row->addWidget(cancel);
    if (m_plan.hasWork()) {
        m_start = new QPushButton(IconFactory::icon(IconKind::Play, Qt::white), tr("&Start sync"), bar);
        m_start->setObjectName(QStringLiteral("Primary"));
        connect(m_start, &QPushButton::clicked, this, &QDialog::accept);
        row->addWidget(m_start);
    }
    return bar;
}

void PreviewDialog::updateStartButton()
{
    if (m_start)
        m_start->setEnabled(!m_confirm || m_confirm->isChecked());
}

} // namespace obor
