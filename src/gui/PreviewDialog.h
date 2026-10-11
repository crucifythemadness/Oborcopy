#pragma once

#include "JobOptions.h"
#include "ResultTreeView.h"
#include "SyncPlan.h"

#include <QDialog>

class QCheckBox;
class QPushButton;
class QVBoxLayout;

namespace obor {

// Shows the plan before anything is changed. In Mirror mode with deletions the
// user must tick a confirmation box before the sync can start.
class PreviewDialog : public QDialog
{
    Q_OBJECT

public:
    PreviewDialog(const SyncPlan &plan, const JobOptions &job, bool previewMode,
                  QWidget *parent = nullptr);

    static QVector<ResultRow> rowsFromPlan(const SyncPlan &plan);

private:
    QWidget *buildTiles();
    void addWarnings(QVBoxLayout *layout);
    QWidget *buildButtons();
    void updateStartButton();

    const SyncPlan &m_plan;
    const JobOptions &m_job;
    QCheckBox *m_confirm = nullptr;
    QPushButton *m_start = nullptr;
};

} // namespace obor
