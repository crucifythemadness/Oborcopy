#pragma once

#include "ResultTreeView.h"
#include "RunReport.h"

#include <QDialog>

class QTextBrowser;

namespace obor {

// Detailed result of a run: counters, a folder tree of everything that was
// copied / updated / deleted and full error details.
class SummaryDialog : public QDialog
{
    Q_OBJECT

public:
    explicit SummaryDialog(RunReport report, QWidget *parent = nullptr);

    static QVector<ResultRow> rowsFromReport(const RunReport &report);

private:
    QWidget *buildHeader();
    QWidget *buildTiles();
    QWidget *buildButtons();
    void showDetails(const ResultRow *row);
    void exportCsv();

    RunReport m_report;
    QTextBrowser *m_details;
};

} // namespace obor
