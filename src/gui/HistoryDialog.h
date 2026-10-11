#pragma once

#include <QDialog>
#include <QStringList>

class QTableWidget;

namespace obor {

// Lists previous runs (from the .json reports next to the log files) and
// opens their detailed summary or log file.
class HistoryDialog : public QDialog
{
    Q_OBJECT

public:
    HistoryDialog(const QStringList &logDirs, QWidget *parent = nullptr);

private:
    void load();
    QStringList reportFiles() const;
    void addRow(const QString &path);
    QString selectedPath() const;
    void openSummary();
    void openLog();

    QStringList m_dirs;
    QTableWidget *m_table;
};

} // namespace obor
