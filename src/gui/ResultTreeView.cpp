#include "ResultTreeView.h"

#include "GuiUtil.h"
#include "IconFactory.h"
#include "Theme.h"

#include <QHBoxLayout>
#include <QHeaderView>
#include <QLineEdit>
#include <QSet>
#include <QSortFilterProxyModel>
#include <QStandardItemModel>
#include <QToolButton>
#include <QTreeView>
#include <QVBoxLayout>

namespace obor {

namespace {

constexpr int KindRole = Qt::UserRole + 1;  // int(ItemKind), -1 for plain folders
constexpr int IndexRole = Qt::UserRole + 2; // index into m_rows, -1 for plain folders
constexpr int PathRole = Qt::UserRole + 3;
constexpr int kAutoExpandLimit = 3000;

const ItemKind kChipOrder[] = {ItemKind::Error,      ItemKind::Copied,     ItemKind::Updated,
                               ItemKind::Deleted,    ItemKind::DirCreated, ItemKind::DirDeleted,
                               ItemKind::Skipped};

} // namespace

// Shows rows whose kind is enabled and whose path matches the search text;
// folders appear automatically when one of their children is shown.
class ResultFilterProxy : public QSortFilterProxyModel
{
public:
    using QSortFilterProxyModel::QSortFilterProxyModel;

    void setFilter(const QSet<int> &kinds, const QString &text)
    {
#if QT_VERSION >= QT_VERSION_CHECK(6, 10, 0)
        beginFilterChange();
        m_kinds = kinds;
        m_search = text;
        endFilterChange(QSortFilterProxyModel::Direction::Rows);
#else
        m_kinds = kinds;
        m_search = text;
        invalidateFilter();
#endif
    }

protected:
    bool filterAcceptsRow(int row, const QModelIndex &parent) const override
    {
        const QModelIndex index = sourceModel()->index(row, 0, parent);
        const int kind = index.data(KindRole).toInt();
        if (kind < 0 || !m_kinds.contains(kind))
            return false;
        return m_search.isEmpty()
               || index.data(PathRole).toString().contains(m_search, Qt::CaseInsensitive);
    }

private:
    QSet<int> m_kinds;
    QString m_search;
};

ResultTreeView::ResultTreeView(QWidget *parent)
    : QWidget(parent)
    , m_model(new QStandardItemModel(this))
    , m_proxy(new ResultFilterProxy(this))
    , m_tree(new QTreeView(this))
    , m_search(new QLineEdit(this))
{
    m_proxy->setSourceModel(m_model);
    m_proxy->setRecursiveFilteringEnabled(true);
    m_tree->setModel(m_proxy);
    m_tree->setAlternatingRowColors(true);
    m_tree->setUniformRowHeights(true);
    m_tree->setSortingEnabled(true);
    m_tree->sortByColumn(-1, Qt::AscendingOrder);
    m_tree->setEditTriggers(QAbstractItemView::NoEditTriggers);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(buildToolbar());
    layout->addWidget(m_tree, 1);

    connect(m_search, &QLineEdit::textChanged, this, [this]() { applyFilter(); });
    connect(m_tree->selectionModel(), &QItemSelectionModel::currentChanged, this,
            &ResultTreeView::onCurrentChanged);
}

QWidget *ResultTreeView::buildToolbar()
{
    auto *bar = new QWidget(this);
    auto *row = new QHBoxLayout(bar);
    row->setContentsMargins(0, 0, 0, 4);
    row->setSpacing(6);
    for (ItemKind kind : kChipOrder) {
        auto *chip = new QToolButton(bar);
        chip->setObjectName(QStringLiteral("Chip"));
        chip->setCheckable(true);
        chip->setChecked(kind != ItemKind::Skipped);
        chip->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
        connect(chip, &QToolButton::toggled, this, [this]() { applyFilter(); });
        m_chips.insert(kind, chip);
        row->addWidget(chip);
    }
    row->addStretch();
    m_search->setPlaceholderText(tr("Search…"));
    m_search->setClearButtonEnabled(true);
    m_search->setMaximumWidth(220);
    auto *expand = new QToolButton(bar);
    expand->setText(tr("Expand all"));
    auto *collapse = new QToolButton(bar);
    collapse->setText(tr("Collapse"));
    connect(expand, &QToolButton::clicked, m_tree, &QTreeView::expandAll);
    connect(collapse, &QToolButton::clicked, m_tree, &QTreeView::collapseAll);
    row->addWidget(m_search);
    row->addWidget(expand);
    row->addWidget(collapse);
    return bar;
}

QString ResultTreeView::actionLabel(ItemKind kind, bool preview)
{
    if (!preview)
        return toString(kind);
    switch (kind) {
    case ItemKind::Copied: return tr("New");
    case ItemKind::Updated: return tr("Update");
    case ItemKind::Deleted: return tr("Delete");
    case ItemKind::DirCreated: return tr("New folder");
    case ItemKind::DirDeleted: return tr("Delete folder");
    case ItemKind::Skipped: return tr("Unchanged");
    case ItemKind::Error: return tr("Error");
    }
    return {};
}

void ResultTreeView::setRows(QVector<ResultRow> rows, bool preview)
{
    m_rows = std::move(rows);
    m_preview = preview;
    updateChips();
    rebuildModel();
}

void ResultTreeView::setKindVisible(ItemKind kind, bool visible)
{
    if (auto *chip = m_chips.value(kind))
        chip->setChecked(visible);
}

void ResultTreeView::updateChips()
{
    QMap<ItemKind, int> counts;
    for (const ResultRow &row : std::as_const(m_rows))
        ++counts[row.kind];
    for (auto it = m_chips.begin(); it != m_chips.end(); ++it) {
        const int n = counts.value(it.key());
        it.value()->setText(QStringLiteral("%1  %2").arg(actionLabel(it.key(), m_preview),
                                                         GuiUtil::formatCount(n)));
        it.value()->setIcon(IconFactory::dot(Theme::colorFor(it.key())));
        it.value()->setVisible(n > 0);
    }
}

void ResultTreeView::rebuildModel()
{
    m_model->clear();
    m_model->setHorizontalHeaderLabels({tr("Name"), tr("Action"), tr("Size"), tr("Details")});
    QHash<QString, QStandardItem *> folders;
    for (int i = 0; i < m_rows.size(); ++i) {
        const ResultRow &row = m_rows.at(i);
        const int slash = int(row.relPath.lastIndexOf(QLatin1Char('/')));
        QStandardItem *parent = slash < 0 ? m_model->invisibleRootItem()
                                          : folderItem(row.relPath.left(slash), folders);
        const bool isDir = row.kind == ItemKind::DirCreated || row.kind == ItemKind::DirDeleted;
        if (QStandardItem *existing = isDir ? folders.value(row.relPath) : nullptr) {
            updateFolderRow(parent, existing, row, i); // node exists because of its children
            continue;
        }
        const QList<QStandardItem *> cells = makeRow(row, i);
        parent->appendRow(cells);
        if (isDir)
            folders.insert(row.relPath, cells.first());
    }
    applyFilter();
    m_tree->header()->setSectionResizeMode(0, QHeaderView::Interactive);
    m_tree->setColumnWidth(0, 360);
}

QStandardItem *ResultTreeView::folderItem(const QString &path,
                                          QHash<QString, QStandardItem *> &folders)
{
    if (QStandardItem *found = folders.value(path))
        return found;
    const int slash = int(path.lastIndexOf(QLatin1Char('/')));
    QStandardItem *parent = slash < 0 ? m_model->invisibleRootItem()
                                      : folderItem(path.left(slash), folders);
    auto *item = new QStandardItem(IconFactory::icon(IconKind::Folder), path.mid(slash + 1));
    item->setData(-1, KindRole);
    item->setData(-1, IndexRole);
    item->setData(path, PathRole);
    parent->appendRow({item, new QStandardItem, new QStandardItem, new QStandardItem});
    folders.insert(path, item);
    return item;
}

QList<QStandardItem *> ResultTreeView::makeRow(const ResultRow &row, int index) const
{
    const QColor color = Theme::colorFor(row.kind);
    const bool isDir = row.kind == ItemKind::DirCreated || row.kind == ItemKind::DirDeleted;
    const QString name = row.relPath.mid(row.relPath.lastIndexOf(QLatin1Char('/')) + 1);
    auto *nameItem = new QStandardItem(isDir ? IconFactory::icon(IconKind::Folder, color)
                                             : IconFactory::dot(color),
                                       name.isEmpty() ? row.relPath : name);
    nameItem->setData(int(row.kind), KindRole);
    nameItem->setData(index, IndexRole);
    nameItem->setData(row.relPath, PathRole);
    nameItem->setToolTip(row.relPath);
    auto *action = new QStandardItem(actionLabel(row.kind, m_preview));
    action->setForeground(color);
    auto *size = new QStandardItem(row.size > 0 ? GuiUtil::formatBytes(row.size) : QString());
    size->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
    QString details = row.message;
    if (row.kind == ItemKind::Error)
        details = tr("%1: %2").arg(row.operation, row.message);
    return {nameItem, action, size, new QStandardItem(details)};
}

void ResultTreeView::updateFolderRow(QStandardItem *parent, QStandardItem *folder,
                                     const ResultRow &row, int index) const
{
    const QList<QStandardItem *> cells = makeRow(row, index);
    folder->setIcon(cells.at(0)->icon());
    folder->setData(int(row.kind), KindRole);
    folder->setData(index, IndexRole);
    for (int c = 1; c < cells.size(); ++c) {
        QStandardItem *target = parent->child(folder->row(), c);
        target->setText(cells.at(c)->text());
        target->setForeground(cells.at(c)->foreground());
    }
    qDeleteAll(cells);
}

void ResultTreeView::applyFilter()
{
    QSet<int> kinds;
    for (auto it = m_chips.cbegin(); it != m_chips.cend(); ++it) {
        if (it.value()->isChecked())
            kinds.insert(int(it.key()));
    }
    m_proxy->setFilter(kinds, m_search->text().trimmed());
    if (m_proxy->rowCount() > 0 && m_rows.size() <= kAutoExpandLimit)
        m_tree->expandAll();
    else
        m_tree->expandToDepth(0);
}

void ResultTreeView::onCurrentChanged()
{
    const QModelIndex index = m_proxy->mapToSource(m_tree->currentIndex());
    const QModelIndex first = index.sibling(index.row(), 0);
    const int row = first.isValid() ? first.data(IndexRole).toInt() : -1;
    emit currentRowChanged(row >= 0 && row < m_rows.size() ? &m_rows.at(row) : nullptr);
}

} // namespace obor
