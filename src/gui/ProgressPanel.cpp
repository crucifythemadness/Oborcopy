#include "ProgressPanel.h"

#include "GuiUtil.h"
#include "IconFactory.h"
#include "Theme.h"

#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPlainTextEdit>
#include <QProgressBar>
#include <QPushButton>
#include <QVBoxLayout>

namespace obor {

ProgressPanel::ProgressPanel(QWidget *parent)
    : QWidget(parent)
{
    auto *card = GuiUtil::makeCard(this);
    auto *cardLayout = new QVBoxLayout(card);
    cardLayout->setContentsMargins(18, 16, 18, 16);
    cardLayout->setSpacing(10);
    cardLayout->addWidget(buildHeader());
    m_overall = new QProgressBar(card);
    m_overall->setRange(0, 1000);
    cardLayout->addWidget(m_overall);
    cardLayout->addWidget(buildStats());
    m_currentFile = new ElidedLabel(card);
    m_currentFile->setObjectName(QStringLiteral("Muted"));
    m_fileBar = new QProgressBar(card);
    m_fileBar->setObjectName(QStringLiteral("Small"));
    m_fileBar->setRange(0, 1000);
    cardLayout->addWidget(m_currentFile);
    cardLayout->addWidget(m_fileBar);
    cardLayout->addWidget(buildCounters());

    m_log = new QPlainTextEdit(this);
    m_log->setReadOnly(true);
    m_log->setMaximumBlockCount(2000);
    m_log->setLineWrapMode(QPlainTextEdit::NoWrap);
    QFont mono(QStringLiteral("Consolas"));
    mono.setStyleHint(QFont::Monospace);
    mono.setPointSizeF(font().pointSizeF() * 0.9);
    m_log->setFont(mono);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(10);
    layout->addWidget(buildResultBanner());
    layout->addWidget(card);
    layout->addWidget(GuiUtil::makeLabel(tr("Activity"), QStringLiteral("SectionTitle"), this));
    layout->addWidget(m_log, 1);
}

QWidget *ProgressPanel::buildHeader()
{
    auto *header = new QWidget(this);
    m_title = GuiUtil::makeLabel(QString(), QStringLiteral("Title"), header);
    m_phase = GuiUtil::makeLabel(QString(), QStringLiteral("Muted"), header);
    m_state = new QLabel(header);
    m_percent = GuiUtil::makeLabel(QString(), QStringLiteral("StatValue"), header);
    auto *text = new QVBoxLayout;
    text->setSpacing(2);
    text->addWidget(m_title);
    text->addWidget(m_phase);
    auto *layout = new QHBoxLayout(header);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addLayout(text, 1);
    layout->addWidget(m_state);
    layout->addWidget(m_percent);
    return header;
}

QLabel *ProgressPanel::addStat(QGridLayout *grid, int column, const QString &caption)
{
    auto *value = new QLabel(QStringLiteral("–"));
    QFont f = value->font();
    f.setBold(true);
    value->setFont(f);
    grid->addWidget(GuiUtil::makeLabel(caption, QStringLiteral("Muted")), 0, column);
    grid->addWidget(value, 1, column);
    return value;
}

QWidget *ProgressPanel::buildStats()
{
    auto *stats = new QWidget(this);
    auto *grid = new QGridLayout(stats);
    grid->setContentsMargins(0, 0, 0, 0);
    grid->setHorizontalSpacing(24);
    m_files = addStat(grid, 0, tr("Files"));
    m_data = addStat(grid, 1, tr("Data"));
    m_speed = addStat(grid, 2, tr("Speed"));
    m_eta = addStat(grid, 3, tr("Time left"));
    for (int c = 0; c < 4; ++c)
        grid->setColumnStretch(c, 1);
    return stats;
}

QLabel *ProgressPanel::addCounter(QHBoxLayout *row, const QColor &color, const QString &caption)
{
    auto *icon = new QLabel;
    icon->setPixmap(IconFactory::dot(color).pixmap(10, 10));
    auto *label = new QLabel;
    label->setProperty("caption", caption);
    row->addWidget(icon);
    row->addWidget(label);
    row->addSpacing(14);
    return label;
}

QWidget *ProgressPanel::buildCounters()
{
    auto *counters = new QWidget(this);
    auto *row = new QHBoxLayout(counters);
    row->setContentsMargins(0, 4, 0, 0);
    const ThemeColors &c = Theme::colors();
    m_copied = addCounter(row, c.success, tr("copied"));
    m_updated = addCounter(row, c.update, tr("updated"));
    m_deleted = addCounter(row, c.danger, tr("deleted"));
    m_errors = addCounter(row, c.warning, tr("errors"));
    row->addStretch();
    return counters;
}

QWidget *ProgressPanel::buildResultBanner()
{
    m_banner = GuiUtil::makeCard(this);
    m_bannerIcon = new QLabel(m_banner);
    m_bannerText = new QLabel(m_banner);
    m_bannerText->setWordWrap(true);
    m_bannerText->setTextInteractionFlags(Qt::TextSelectableByMouse);
    auto *details = new QPushButton(IconFactory::icon(IconKind::Document), tr("Show details"), m_banner);
    details->setObjectName(QStringLiteral("Primary"));
    auto *log = new QPushButton(tr("Open log"), m_banner);
    auto *done = new QPushButton(tr("Back to options"), m_banner);
    auto *layout = new QHBoxLayout(m_banner);
    layout->setContentsMargins(16, 12, 16, 12);
    layout->addWidget(m_bannerIcon);
    layout->addWidget(m_bannerText, 1);
    layout->addWidget(details);
    layout->addWidget(log);
    layout->addWidget(done);
    connect(details, &QPushButton::clicked, this, &ProgressPanel::detailsRequested);
    connect(log, &QPushButton::clicked, this, &ProgressPanel::openLogRequested);
    connect(done, &QPushButton::clicked, this, &ProgressPanel::doneRequested);
    m_banner->setVisible(false);
    return m_banner;
}

void ProgressPanel::startRun(const QString &jobName, bool preview)
{
    m_title->setText(preview ? tr("Preview: %1").arg(jobName) : jobName);
    m_phase->setText(toString(SyncPhase::Validating));
    m_state->clear();
    m_percent->clear();
    m_overall->setRange(0, 0); // busy indicator until totals are known
    m_fileBar->setValue(0);
    m_currentFile->setFullText(QString());
    m_banner->setVisible(false);
    m_log->clear();
    m_paused = false;
    updateStats(ProgressInfo());
}

void ProgressPanel::updateProgress(const ProgressInfo &info)
{
    QString phase = toString(info.phase);
    if (info.phase == SyncPhase::Scanning)
        phase += tr(" – %1 entries").arg(GuiUtil::formatCount(info.scanned));
    m_phase->setText(m_paused ? tr("Paused – %1").arg(phase) : phase);
    const bool copying = info.phase == SyncPhase::Copying || info.phase == SyncPhase::Finalizing;
    if (copying && info.bytesTotal > 0) {
        const int permille = int(1000.0 * double(info.bytesDone) / double(info.bytesTotal));
        m_overall->setRange(0, 1000);
        m_overall->setValue(qBound(0, permille, 1000));
        m_percent->setText(QStringLiteral("%1 %").arg(permille / 10));
    } else if (copying || info.phase == SyncPhase::Finished) {
        m_overall->setRange(0, 1000);
        m_overall->setValue(1000);
    } else {
        m_overall->setRange(0, 0);
    }
    m_currentFile->setFullText(info.currentFile);
    m_fileBar->setValue(info.currentSize > 0
                            ? int(1000.0 * double(info.currentBytes) / double(info.currentSize)) : 0);
    updateStats(info);
}

void ProgressPanel::updateStats(const ProgressInfo &info)
{
    m_files->setText(tr("%1 / %2").arg(GuiUtil::formatCount(info.filesDone),
                                       GuiUtil::formatCount(info.filesTotal)));
    m_data->setText(tr("%1 / %2").arg(GuiUtil::formatBytes(info.bytesDone),
                                      GuiUtil::formatBytes(info.bytesTotal)));
    m_speed->setText(info.bytesPerSecond > 0
                         ? tr("%1/s").arg(GuiUtil::formatBytes(qint64(info.bytesPerSecond)))
                         : QStringLiteral("–"));
    m_eta->setText(GuiUtil::formatDuration(info.etaSeconds));
    const std::pair<QLabel *, int> counters[] = {{m_copied, info.copied}, {m_updated, info.updated},
                                                 {m_deleted, info.deleted}, {m_errors, info.errors}};
    for (const auto &[label, value] : counters)
        label->setText(QStringLiteral("%1 %2").arg(GuiUtil::formatCount(value),
                                                   label->property("caption").toString()));
}

void ProgressPanel::setPaused(bool paused)
{
    m_paused = paused;
    m_state->setText(paused ? tr("⏸ Paused") : QString());
}

void ProgressPanel::appendLog(const QString &line)
{
    m_log->appendPlainText(line);
}

void ProgressPanel::showResult(const RunReport &report)
{
    const ThemeColors &c = Theme::colors();
    IconKind icon = IconKind::Check;
    QColor color = c.success;
    if (report.status == RunStatus::Failed || report.status == RunStatus::CompletedWithErrors) {
        icon = report.status == RunStatus::Failed ? IconKind::Error : IconKind::Warning;
        color = report.status == RunStatus::Failed ? c.danger : c.warning;
    } else if (report.status == RunStatus::Cancelled) {
        icon = IconKind::Stop;
        color = c.muted;
    }
    m_bannerIcon->setPixmap(IconFactory::icon(icon, color).pixmap(32, 32));
    QString text = QStringLiteral("<b>%1</b> – %2").arg(toString(report.status), report.summaryLine());
    if (!report.fatalError.isEmpty())
        text += QStringLiteral("<br>%1").arg(report.fatalError.toHtmlEscaped());
    m_bannerText->setText(text);
    m_banner->setVisible(true);
    m_state->clear();
    m_phase->setText(tr("Finished after %1").arg(GuiUtil::formatDuration(report.durationMs() / 1000)));
    m_overall->setRange(0, 1000);
    m_overall->setValue(report.status == RunStatus::Completed ? 1000 : m_overall->value());
    m_fileBar->setValue(0);
    m_currentFile->setFullText(QString());
}

} // namespace obor
