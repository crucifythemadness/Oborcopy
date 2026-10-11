#pragma once

#include "RunReport.h"

#include <QHash>
#include <QMap>
#include <QVector>
#include <QWidget>

class QLineEdit;
class QStandardItem;
class QStandardItemModel;
class QToolButton;
class QTreeView;

namespace obor {

class ResultFilterProxy;

struct ResultRow
{
    QString relPath;
    ItemKind kind = ItemKind::Copied;
    qint64 size = 0;
    QString message;
    QString operation;
    int attempts = 1;
};

// Folder tree of results (copied / updated / deleted / errors ...) with filter
// chips and a search box. Used by the preview and the summary dialog.
class ResultTreeView : public QWidget
{
    Q_OBJECT

public:
    explicit ResultTreeView(QWidget *parent = nullptr);

    // preview = true labels the actions as planned ("New", "Delete") instead of done.
    void setRows(QVector<ResultRow> rows, bool preview);
    void setKindVisible(ItemKind kind, bool visible);
    const QVector<ResultRow> &rows() const { return m_rows; }

    static QString actionLabel(ItemKind kind, bool preview);

signals:
    void currentRowChanged(const obor::ResultRow *row);

private:
    QWidget *buildToolbar();
    void rebuildModel();
    QStandardItem *folderItem(const QString &path, QHash<QString, QStandardItem *> &folders);
    QList<QStandardItem *> makeRow(const ResultRow &row, int index) const;
    void updateFolderRow(QStandardItem *parent, QStandardItem *folder, const ResultRow &row,
                         int index) const;
    void updateChips();
    void applyFilter();
    void onCurrentChanged();

    QVector<ResultRow> m_rows;
    bool m_preview = false;
    QStandardItemModel *m_model;
    ResultFilterProxy *m_proxy;
    QTreeView *m_tree;
    QLineEdit *m_search;
    QMap<ItemKind, QToolButton *> m_chips;
};

} // namespace obor
