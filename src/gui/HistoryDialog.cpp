#include "HistoryDialog.h"

#include "AppPaths.h"
#include "GuiUtil.h"
#include "IconFactory.h"
#include "JobStore.h"
#include "RunReport.h"
#include "SummaryDialog.h"
#include "Theme.h"

#include <QComboBox>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QJsonDocument>
#include <QLocale>
#include <QPushButton>
#include <QTableWidget>
#include <QVBoxLayout>

#include <algorithm>
#include <optional>

namespace obor {

namespace {

constexpr int PathRole = Qt::UserRole + 1;
constexpr int ScheduledRole = Qt::UserRole + 2;
constexpr int kTriggerColumn = 2;
constexpr int kMaxRuns = 500;

std::optional<RunReport> readReport(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return std::nullopt;
    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    if (!doc.isObject())
        return std::nullopt;
    return RunReport::fromJson(doc.object());
}

} // namespace

HistoryDialog::HistoryDialog(const QStringList &logDirs, QWidget *parent)
    : QDialog(parent)
    , m_dirs(logDirs)
    , m_table(new QTableWidget(this))
    , m_filter(new QComboBox(this))
{
    setWindowTitle(tr("Run history"));
    resize(1060, 540);
    m_table->setColumnCount(6);
    m_table->setHorizontalHeaderLabels(
        {tr("Started"), tr("Job"), tr("Started by"), tr("Mode"), tr("Status"), tr("Result")});
    m_filter->addItems({tr("All runs"), tr("Started manually"), tr("Started by the scheduler")});
    connect(m_filter, &QComboBox::currentIndexChanged, this, &HistoryDialog::applyFilter);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->verticalHeader()->setVisible(false);
    m_table->horizontalHeader()->setStretchLastSection(true);
    m_table->setAlternatingRowColors(true);

    auto *details = new QPushButton(tr("Show details"), this);
    details->setObjectName(QStringLiteral("Primary"));
    auto *log = new QPushButton(tr("Open log file"), this);
    auto *close = new QPushButton(tr("Close"), this);
    auto *buttons = new QHBoxLayout;
    buttons->addWidget(details);
    buttons->addWidget(log);
    buttons->addStretch();
    buttons->addWidget(close);

    auto *layout = new QVBoxLayout(this);
    auto *header = new QHBoxLayout;
    header->addWidget(GuiUtil::makeLabel(tr("Previous runs"), QStringLiteral("Title"), this), 1);
    header->addWidget(m_filter);
    layout->addLayout(header);
    layout->addWidget(m_table, 1);
    layout->addLayout(buttons);

    connect(details, &QPushButton::clicked, this, &HistoryDialog::openSummary);
    connect(log, &QPushButton::clicked, this, &HistoryDialog::openLog);
    connect(close, &QPushButton::clicked, this, &QDialog::accept);
    connect(m_table, &QTableWidget::cellDoubleClicked, this, &HistoryDialog::openSummary);
    load();
}

QStringList HistoryDialog::reportFiles() const
{
    QFileInfoList infos;
    for (const QString &dir : m_dirs)
        infos += QDir(dir).entryInfoList({QStringLiteral("Oborcopy_*.json")}, QDir::Files);
    std::sort(infos.begin(), infos.end(), [](const QFileInfo &a, const QFileInfo &b) {
        return a.lastModified() > b.lastModified();
    });
    QStringList paths;
    for (const QFileInfo &info : std::as_const(infos)) {
        if (!paths.contains(info.absoluteFilePath()))
            paths << info.absoluteFilePath();
        if (paths.size() >= kMaxRuns)
            break;
    }
    return paths;
}

void HistoryDialog::load()
{
    m_table->setRowCount(0);
    for (const QString &path : reportFiles())
        addRow(path);
    m_table->resizeColumnsToContents();
    applyFilter();
}

void HistoryDialog::setFilter(Filter filter)
{
    m_filter->setCurrentIndex(int(filter));
}

void HistoryDialog::applyFilter()
{
    const auto filter = Filter(m_filter->currentIndex());
    int first = -1;
    for (int row = 0; row < m_table->rowCount(); ++row) {
        const bool scheduled = m_table->item(row, 0)->data(ScheduledRole).toBool();
        const bool visible = filter == Filter::All || (filter == Filter::Scheduled) == scheduled;
        m_table->setRowHidden(row, !visible);
        if (visible && first < 0)
            first = row;
    }
    if (first >= 0)
        m_table->selectRow(first);
    else
        m_table->clearSelection();
}

QStringList HistoryDialog::logDirsFor(const JobStore &store)
{
    QStringList dirs{AppPaths::defaultLogDir()};
    for (const QString &name : store.jobNames()) {
        const auto job = store.load(name);
        if (job && !job->log.logDir.isEmpty() && !dirs.contains(job->log.logDir))
            dirs << job->log.logDir;
    }
    return dirs;
}

void HistoryDialog::addRow(const QString &path)
{
    const auto report = readReport(path);
    if (!report)
        return;
    const int row = m_table->rowCount();
    m_table->insertRow(row);
    const QString mode = toString(report->mode) + (report->dryRun ? tr(" (preview)") : QString());
    const QStringList cells = {QLocale().toString(report->started, QLocale::ShortFormat),
                               report->jobName,
                               report->scheduled ? tr("Scheduler") : tr("You"),
                               mode,
                               toString(report->status),
                               report->summaryLine()};
    for (int c = 0; c < cells.size(); ++c) {
        auto *item = new QTableWidgetItem(cells.at(c));
        item->setData(PathRole, path);
        item->setData(ScheduledRole, report->scheduled);
        m_table->setItem(row, c, item);
    }
    if (report->scheduled)
        m_table->item(row, kTriggerColumn)->setIcon(IconFactory::icon(IconKind::History, Theme::colors().accent));
    if (report->status != RunStatus::Completed)
        m_table->item(row, 4)->setForeground(report->status == RunStatus::Cancelled
                                                 ? Theme::colors().muted : Theme::colors().warning);
}

QString HistoryDialog::selectedPath() const
{
    const auto *item = m_table->item(m_table->currentRow(), 0);
    return item ? item->data(PathRole).toString() : QString();
}

void HistoryDialog::openSummary()
{
    const auto report = readReport(selectedPath());
    if (!report)
        return;
    SummaryDialog dialog(*report, this);
    dialog.exec();
}

void HistoryDialog::openLog()
{
    QString path = selectedPath();
    if (path.isEmpty())
        return;
    path.chop(5); // ".json"
    GuiUtil::openFile(path + QStringLiteral(".log"));
}

} // namespace obor
