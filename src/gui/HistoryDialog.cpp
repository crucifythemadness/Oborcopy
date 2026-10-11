#include "HistoryDialog.h"

#include "GuiUtil.h"
#include "RunReport.h"
#include "SummaryDialog.h"
#include "Theme.h"

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
{
    setWindowTitle(tr("Run history"));
    resize(980, 520);
    m_table->setColumnCount(5);
    m_table->setHorizontalHeaderLabels({tr("Started"), tr("Job"), tr("Mode"), tr("Status"), tr("Result")});
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
    layout->addWidget(GuiUtil::makeLabel(tr("Previous runs"), QStringLiteral("Title"), this));
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
    if (m_table->rowCount() > 0)
        m_table->selectRow(0);
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
                               report->jobName, mode, toString(report->status), report->summaryLine()};
    for (int c = 0; c < cells.size(); ++c) {
        auto *item = new QTableWidgetItem(cells.at(c));
        item->setData(PathRole, path);
        m_table->setItem(row, c, item);
    }
    if (report->status != RunStatus::Completed)
        m_table->item(row, 3)->setForeground(report->status == RunStatus::Cancelled
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
