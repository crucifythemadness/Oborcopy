#pragma once

#include "JobOptions.h"

#include <QWidget>

class QCheckBox;
class QComboBox;
class QLineEdit;
class QSpinBox;

namespace obor {

class ModeCard;

// The options most users need: mode, subfolders, retries, tolerance, excludes, preview.
class BasicOptionsPage : public QWidget
{
    Q_OBJECT

public:
    explicit BasicOptionsPage(QWidget *parent = nullptr);

    void load(const JobOptions &job);
    void store(JobOptions &job) const;

signals:
    void changed();

private:
    QWidget *buildModeCards();
    QWidget *buildOptions();
    void setMode(SyncMode mode);
    void updateToleranceState();

    ModeCard *m_updateCard;
    ModeCard *m_mirrorCard;
    SyncMode m_mode = SyncMode::Update;
    QCheckBox *m_recursive;
    QSpinBox *m_retries;
    QComboBox *m_tolerance;
    QSpinBox *m_toleranceMs;
    QLineEdit *m_excludeFiles;
    QLineEdit *m_excludeDirs;
    QCheckBox *m_dryRun;
};

} // namespace obor
