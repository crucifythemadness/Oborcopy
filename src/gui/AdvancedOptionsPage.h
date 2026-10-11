#pragma once

#include "JobOptions.h"

#include <QWidget>

class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class QFormLayout;
class QLineEdit;
class QSpinBox;

namespace obor {

class CollapsibleSection;

// All remaining options, grouped into collapsible sections.
class AdvancedOptionsPage : public QWidget
{
    Q_OBJECT

public:
    explicit AdvancedOptionsPage(QWidget *parent = nullptr);

    void load(const JobOptions &job);
    void store(JobOptions &job) const;

signals:
    void changed();

private:
    CollapsibleSection *buildCopySection();
    CollapsibleSection *buildCompareSection();
    CollapsibleSection *buildFilterSection();
    CollapsibleSection *buildMetadataSection();
    CollapsibleSection *buildRetrySection();
    CollapsibleSection *buildSafetySection();
    CollapsibleSection *buildLogSection();
    CollapsibleSection *section(const QString &title, const QString &subtitle, QFormLayout *form);
    void connectChangeSignals();

    void loadCopy(const CopyOptions &c);
    void loadFilter(const FilterOptions &f);
    void loadOther(const JobOptions &job);
    void storeCopy(CopyOptions &c) const;
    void storeFilter(FilterOptions &f) const;
    void storeOther(JobOptions &job) const;

    // Copy behaviour
    QCheckBox *m_copyEmptyDirs;
    QSpinBox *m_maxDepth;
    QCheckBox *m_moveFiles;
    QCheckBox *m_skipOlder;
    QCheckBox *m_skipNewer;
    QCheckBox *m_overwriteAll;
    QSpinBox *m_parallel;
    QSpinBox *m_throughput;
    // Comparison
    QCheckBox *m_dst;
    QCheckBox *m_checksum;
    QCheckBox *m_verify;
    // Filters
    QLineEdit *m_includeFiles;
    QDoubleSpinBox *m_minSize;
    QDoubleSpinBox *m_maxSize;
    QSpinBox *m_minAge;
    QSpinBox *m_maxAge;
    QCheckBox *m_skipHidden;
    QCheckBox *m_skipSystem;
    QComboBox *m_symlinks;
    // Metadata
    QCheckBox *m_fileTimes;
    QCheckBox *m_dirTimes;
    QCheckBox *m_attributes;
    QCheckBox *m_permissions;
    // Retry
    QDoubleSpinBox *m_baseWait;
    QDoubleSpinBox *m_maxWait;
    // Safety
    QComboBox *m_deleteMode;
    QCheckBox *m_thresholdEnabled;
    QSpinBox *m_thresholdPercent;
    QCheckBox *m_refuseEmpty;
    // Logging
    QLineEdit *m_logDir;
    QComboBox *m_logLevel;
    QSpinBox *m_keepLogs;
};

} // namespace obor
