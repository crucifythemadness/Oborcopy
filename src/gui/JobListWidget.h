#pragma once

#include <QHash>
#include <QWidget>

class QListWidget;
class QListWidgetItem;
class QToolButton;

namespace obor {

class JobStore;

// Sidebar list of saved jobs with new / duplicate / rename / delete actions.
class JobListWidget : public QWidget
{
    Q_OBJECT

public:
    JobListWidget(JobStore &store, QWidget *parent = nullptr);

    void reload(const QString &select = QString());
    QString currentJob() const;
    void setLocked(bool locked);
    // Jobs run by the scheduler service get a clock icon; value = schedule description.
    void setScheduled(const QHash<QString, QString> &scheduled);

public slots:
    void createJob();
    void duplicateJob();
    void renameJob();
    void deleteJob();

signals:
    // Emitted before the store is modified, so pending edits can be saved first.
    void aboutToModify();
    void jobSelected(const QString &name);
    void jobRenamed(const QString &oldName, const QString &newName);
    void jobDeleted(const QString &name);

private:
    QToolButton *makeButton(int icon, const QString &tip);
    QString askName(const QString &title, const QString &initial);
    QString uniqueName(const QString &base) const;
    void ensureOneJob();
    void decorate(QListWidgetItem *item) const;

    JobStore &m_store;
    QListWidget *m_list;
    QList<QToolButton *> m_buttons;
    QHash<QString, QString> m_scheduled;
};

} // namespace obor
