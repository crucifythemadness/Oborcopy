#include "SummaryDialog.h"

#include "GuiUtil.h"
#include "IconFactory.h"
#include "Theme.h"

#include <QDir>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QSaveFile>
#include <QSplitter>
#include <QTextBrowser>
#include <QVBoxLayout>

namespace obor {

SummaryDialog::SummaryDialog(RunReport report, QWidget *parent)
    : QDialog(parent)
    , m_report(std::move(report))
    , m_details(new QTextBrowser(this))
{
    setWindowTitle(tr("Summary – %1").arg(m_report.jobName));
    resize(1020, 740);

    auto *tree = new ResultTreeView(this);
    tree->setRows(rowsFromReport(m_report), m_report.dryRun);
    m_details->setPlaceholderText(tr("Select an item to see its details."));

    auto *splitter = new QSplitter(Qt::Vertical, this);
    splitter->addWidget(tree);
    splitter->addWidget(m_details);
    splitter->setStretchFactor(0, 4);
    splitter->setStretchFactor(1, 1);

    auto *layout = new QVBoxLayout(this);
    layout->setSpacing(10);
    layout->addWidget(buildHeader());
    layout->addWidget(buildTiles());
    if (!m_report.fatalError.isEmpty()) {
        layout->addWidget(GuiUtil::makeBanner(IconFactory::icon(IconKind::Error, Theme::colors().danger),
                                              m_report.fatalError.toHtmlEscaped(),
                                              Theme::colors().danger, this));
    }
    layout->addWidget(splitter, 1);
    layout->addWidget(buildButtons());

    connect(tree, &ResultTreeView::currentRowChanged, this, &SummaryDialog::showDetails);
}

QVector<ResultRow> SummaryDialog::rowsFromReport(const RunReport &report)
{
    QVector<ResultRow> rows;
    rows.reserve(report.items.size());
    for (const ReportItem &item : report.items)
        rows.append({item.relPath, item.kind, item.size, item.message, item.operation, item.attempts});
    return rows;
}

QWidget *SummaryDialog::buildHeader()
{
    auto *header = new QWidget(this);
    auto *title = GuiUtil::makeLabel(QStringLiteral("%1 – %2").arg(m_report.jobName, toString(m_report.status)),
                                     QStringLiteral("Title"), header);
    const QString when = QLocale().toString(m_report.started, QLocale::ShortFormat);
    auto *meta = GuiUtil::makeLabel(
        tr("%1 · %2 · %3 → %4 · duration %5")
            .arg(when, toString(m_report.mode) + (m_report.dryRun ? tr(" (preview)") : QString()),
                 GuiUtil::displayPath(m_report.source), GuiUtil::displayPath(m_report.destination),
                 GuiUtil::formatDuration(m_report.durationMs() / 1000)),
        QStringLiteral("Muted"), header);
    meta->setWordWrap(true);
    auto *line = GuiUtil::makeLabel(m_report.summaryLine(), QStringLiteral("SectionTitle"), header);
    auto *layout = new QVBoxLayout(header);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(title);
    layout->addWidget(meta);
    layout->addWidget(line);
    return header;
}

QWidget *SummaryDialog::buildTiles()
{
    const ThemeColors &c = Theme::colors();
    const RunStats &s = m_report.stats;
    auto *tiles = new QWidget(this);
    auto *row = new QHBoxLayout(tiles);
    row->setContentsMargins(0, 0, 0, 0);
    row->addWidget(GuiUtil::makeStatTile(GuiUtil::formatCount(s.filesChecked), tr("checked"), c.text));
    row->addWidget(GuiUtil::makeStatTile(GuiUtil::formatCount(s.copied), tr("copied"), c.success));
    row->addWidget(GuiUtil::makeStatTile(GuiUtil::formatCount(s.updated), tr("updated"), c.update));
    row->addWidget(GuiUtil::makeStatTile(GuiUtil::formatCount(s.deleted), tr("deleted"), c.danger));
    row->addWidget(GuiUtil::makeStatTile(GuiUtil::formatCount(s.errors), tr("errors"), c.warning));
    row->addWidget(GuiUtil::makeStatTile(GuiUtil::formatBytes(s.bytesCopied), tr("data copied"), c.accent));
    return tiles;
}

QWidget *SummaryDialog::buildButtons()
{
    auto *bar = new QWidget(this);
    auto *row = new QHBoxLayout(bar);
    row->setContentsMargins(0, 0, 0, 0);
    auto *log = new QPushButton(IconFactory::icon(IconKind::Document), tr("Open log file"), bar);
    log->setEnabled(QFileInfo::exists(m_report.logFile));
    auto *exportButton = new QPushButton(tr("Export CSV…"), bar);
    auto *close = new QPushButton(tr("Close"), bar);
    close->setObjectName(QStringLiteral("Primary"));
    row->addWidget(log);
    row->addWidget(exportButton);
    row->addStretch();
    row->addWidget(close);
    connect(log, &QPushButton::clicked, this, [this]() { GuiUtil::openFile(m_report.logFile); });
    connect(exportButton, &QPushButton::clicked, this, &SummaryDialog::exportCsv);
    connect(close, &QPushButton::clicked, this, &QDialog::accept);
    return bar;
}

void SummaryDialog::showDetails(const ResultRow *row)
{
    if (!row) {
        m_details->clear();
        return;
    }
    const QString source = QDir::toNativeSeparators(m_report.source + QLatin1Char('/') + row->relPath);
    const QString dest = QDir::toNativeSeparators(m_report.destination + QLatin1Char('/') + row->relPath);
    QString html = QStringLiteral("<b>%1</b> – %2<br>").arg(row->relPath.toHtmlEscaped(),
                                                             ResultTreeView::actionLabel(row->kind, m_report.dryRun));
    html += tr("Source: %1<br>Destination: %2<br>").arg(source.toHtmlEscaped(), dest.toHtmlEscaped());
    if (row->size > 0)
        html += tr("Size: %1<br>").arg(GuiUtil::formatBytes(row->size));
    if (row->kind == ItemKind::Error) {
        html += tr("<span style='color:%1'><b>%2 failed:</b> %3</span><br>Attempts: %4")
                    .arg(Theme::colors().warning.name(), row->operation, row->message.toHtmlEscaped())
                    .arg(row->attempts);
    } else if (!row->message.isEmpty()) {
        html += tr("Reason: %1").arg(row->message.toHtmlEscaped());
    }
    m_details->setHtml(html);
}

void SummaryDialog::exportCsv()
{
    const QString path = GuiUtil::chooseSaveFile(this, tr("Export summary"),
                                                 QDir::homePath() + QStringLiteral("/oborcopy-summary.csv"),
                                                 tr("CSV files (*.csv)"));
    if (path.isEmpty())
        return;
    auto quote = [](QString s) { return QLatin1Char('"') + s.replace(QLatin1Char('"'), QStringLiteral("\"\"")) + QLatin1Char('"'); };
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        QMessageBox::warning(this, tr("Export summary"), file.errorString());
        return;
    }
    file.write("action;path;size;operation;message;attempts\n");
    for (const ReportItem &item : std::as_const(m_report.items)) {
        const QStringList cells = {quote(toString(item.kind)), quote(item.relPath), QString::number(item.size),
                                   quote(item.operation), quote(item.message), QString::number(item.attempts)};
        file.write(cells.join(QLatin1Char(';')).toUtf8() + "\n");
    }
    if (!file.commit())
        QMessageBox::warning(this, tr("Export summary"), file.errorString());
}

} // namespace obor
