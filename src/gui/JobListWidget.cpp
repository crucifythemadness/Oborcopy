#include "JobListWidget.h"

#include "IconFactory.h"
#include "JobStore.h"

#include <QHBoxLayout>
#include <QInputDialog>
#include <QListWidget>
#include <QMessageBox>
#include <QToolButton>
#include <QVBoxLayout>

namespace obor {

JobListWidget::JobListWidget(JobStore &store, QWidget *parent)
    : QWidget(parent)
    , m_store(store)
    , m_list(new QListWidget(this))
{
    m_list->setObjectName(QStringLiteral("JobList"));
    m_list->setIconSize(QSize(18, 18));

    auto *buttons = new QHBoxLayout;
    buttons->setContentsMargins(0, 0, 0, 0);
    auto *add = makeButton(int(IconKind::Add), tr("New job (Ctrl+N)"));
    auto *dup = makeButton(int(IconKind::Duplicate), tr("Duplicate job"));
    auto *ren = makeButton(int(IconKind::Edit), tr("Rename job"));
    auto *del = makeButton(int(IconKind::Trash), tr("Delete job"));
    for (QToolButton *b : {add, dup, ren, del})
        buttons->addWidget(b);
    buttons->addStretch();

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(m_list, 1);
    layout->addLayout(buttons);

    connect(add, &QToolButton::clicked, this, &JobListWidget::createJob);
    connect(dup, &QToolButton::clicked, this, &JobListWidget::duplicateJob);
    connect(ren, &QToolButton::clicked, this, &JobListWidget::renameJob);
    connect(del, &QToolButton::clicked, this, &JobListWidget::deleteJob);
    connect(m_list, &QListWidget::currentTextChanged, this, &JobListWidget::jobSelected);
    connect(m_list, &QListWidget::itemDoubleClicked, this, &JobListWidget::renameJob);
}

QToolButton *JobListWidget::makeButton(int icon, const QString &tip)
{
    auto *button = new QToolButton(this);
    button->setIcon(IconFactory::icon(IconKind(icon)));
    button->setIconSize(QSize(18, 18));
    button->setToolTip(tip);
    m_buttons << button;
    return button;
}

void JobListWidget::reload(const QString &select)
{
    ensureOneJob();
    QSignalBlocker blocker(m_list);
    m_list->clear();
    const QStringList names = m_store.jobNames();
    for (const QString &name : names)
        new QListWidgetItem(IconFactory::icon(IconKind::MirrorMode), name, m_list);
    int row = names.indexOf(select);
    if (row < 0)
        row = 0;
    m_list->setCurrentRow(row);
    blocker.unblock();
    emit jobSelected(currentJob());
}

QString JobListWidget::currentJob() const
{
    const auto *item = m_list->currentItem();
    return item ? item->text() : QString();
}

void JobListWidget::setLocked(bool locked)
{
    m_list->setEnabled(!locked);
    for (QToolButton *button : std::as_const(m_buttons))
        button->setEnabled(!locked);
}

void JobListWidget::createJob()
{
    const QString name = askName(tr("New job"), uniqueName(tr("New job")));
    if (name.isEmpty())
        return;
    emit aboutToModify();
    m_store.save(JobOptions::defaults(name));
    reload(name);
}

void JobListWidget::duplicateJob()
{
    const QString current = currentJob();
    emit aboutToModify();
    auto job = m_store.load(current);
    if (!job)
        return;
    const QString name = askName(tr("Duplicate job"), uniqueName(current + tr(" (copy)")));
    if (name.isEmpty())
        return;
    job->name = name;
    m_store.save(*job);
    reload(name);
}

void JobListWidget::renameJob()
{
    const QString current = currentJob();
    if (current.isEmpty())
        return;
    const QString name = askName(tr("Rename job"), current);
    if (name.isEmpty() || name == current)
        return;
    emit aboutToModify();
    QString error;
    if (!m_store.rename(current, name, &error)) {
        QMessageBox::warning(this, tr("Rename job"), error);
        return;
    }
    emit jobRenamed(current, name);
    reload(name);
}

void JobListWidget::deleteJob()
{
    const QString current = currentJob();
    if (current.isEmpty())
        return;
    const auto answer = QMessageBox::question(
        this, tr("Delete job"),
        tr("Delete the job \"%1\"?\n\nOnly the job settings are removed. No files are touched.")
            .arg(current));
    if (answer != QMessageBox::Yes)
        return;
    emit aboutToModify();
    m_store.remove(current);
    emit jobDeleted(current);
    reload();
}

QString JobListWidget::askName(const QString &title, const QString &initial)
{
    QInputDialog dialog(this);
    dialog.setWindowTitle(title);
    dialog.setLabelText(tr("Job name:"));
    dialog.setTextValue(initial);
    if (auto *edit = dialog.findChild<QLineEdit *>())
        edit->setMinimumWidth(340); // the default dialog is too narrow to show its title
    const bool ok = dialog.exec() == QDialog::Accepted;
    const QString name = dialog.textValue().trimmed();
    if (!ok || name.isEmpty())
        return {};
    if (name.compare(currentJob(), Qt::CaseInsensitive) != 0 && m_store.exists(name)) {
        QMessageBox::warning(this, title, tr("A job named \"%1\" already exists.").arg(name));
        return {};
    }
    return name;
}

QString JobListWidget::uniqueName(const QString &base) const
{
    QString name = base;
    for (int i = 2; m_store.exists(name); ++i)
        name = QStringLiteral("%1 %2").arg(base).arg(i);
    return name;
}

void JobListWidget::ensureOneJob()
{
    if (m_store.jobNames().isEmpty())
        m_store.save(JobOptions::defaults(tr("My Backup")));
}

} // namespace obor
