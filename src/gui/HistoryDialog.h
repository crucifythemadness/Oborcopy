#pragma once

#include <QDialog>
#include <QStringList>

class QComboBox;
class QTableWidget;

namespace obor {

class JobStore;

// Lists previous runs (from the .json reports next to the log files) and
// opens their detailed summary or log file.
class HistoryDialog : public QDialog
{
    Q_OBJECT

public:
    enum class Filter { All, Manual, Scheduled };

    HistoryDialog(const QStringList &logDirs, QWidget *parent = nullptr);

    void setFilter(Filter filter);
    // The default log folder plus every job's own log folder.
    static QStringList logDirsFor(const JobStore &store);

private:
    void load();
    QStringList reportFiles() const;
    void addRow(const QString &path);
    void applyFilter();
    QString selectedPath() const;
    void openSummary();
    void openLog();

    QStringList m_dirs;
    QTableWidget *m_table;
    QComboBox *m_filter;
};

} // namespace obor
