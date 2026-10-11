#pragma once

#include "Progress.h"
#include "RunReport.h"

#include <QWidget>

class QLabel;
class QPlainTextEdit;
class QProgressBar;
class QPushButton;
class QFrame;
class QGridLayout;
class QHBoxLayout;

namespace obor {

class ElidedLabel;

// Live view of a running sync: phase, overall and per-file progress, speed,
// ETA, counters and an activity log. Shows the result banner at the end.
class ProgressPanel : public QWidget
{
    Q_OBJECT

public:
    explicit ProgressPanel(QWidget *parent = nullptr);

    void startRun(const QString &jobName, bool preview);
    void updateProgress(const ProgressInfo &info);
    void setPaused(bool paused);
    void appendLog(const QString &line);
    void showResult(const RunReport &report);

signals:
    void detailsRequested();
    void openLogRequested();
    void doneRequested();

private:
    QWidget *buildHeader();
    QWidget *buildStats();
    QWidget *buildCounters();
    QWidget *buildResultBanner();
    QLabel *addStat(QGridLayout *grid, int column, const QString &caption);
    QLabel *addCounter(QHBoxLayout *row, const QColor &color, const QString &caption);
    void updateStats(const ProgressInfo &info);

    QLabel *m_title;
    QLabel *m_phase;
    QLabel *m_state;
    QProgressBar *m_overall;
    QLabel *m_percent;
    QLabel *m_files;
    QLabel *m_data;
    QLabel *m_speed;
    QLabel *m_eta;
    ElidedLabel *m_currentFile;
    QProgressBar *m_fileBar;
    QLabel *m_copied;
    QLabel *m_updated;
    QLabel *m_deleted;
    QLabel *m_errors;
    QFrame *m_banner;
    QLabel *m_bannerIcon;
    QLabel *m_bannerText;
    QPlainTextEdit *m_log;
    bool m_paused = false;
};

} // namespace obor
