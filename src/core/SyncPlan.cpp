#include "SyncPlan.h"

#include <algorithm>

namespace obor {

int SyncPlan::count(ActionType type) const
{
    const QVector<SyncAction> *list = nullptr;
    switch (type) {
    case ActionType::CreateDir: list = &createDirs; break;
    case ActionType::CopyNew:
    case ActionType::Update: list = &copies; break;
    case ActionType::DeleteFile:
    case ActionType::DeleteDir: list = &deletions; break;
    case ActionType::Skip: return unchanged;
    }
    return int(std::count_if(list->cbegin(), list->cend(),
                             [type](const SyncAction &a) { return a.type == type; }));
}

bool SyncPlan::hasWork() const
{
    return !deletions.isEmpty() || !createDirs.isEmpty() || !copies.isEmpty();
}

QString toString(ActionType type)
{
    switch (type) {
    case ActionType::CreateDir: return QStringLiteral("Create folder");
    case ActionType::CopyNew: return QStringLiteral("Copy");
    case ActionType::Update: return QStringLiteral("Update");
    case ActionType::DeleteFile: return QStringLiteral("Delete");
    case ActionType::DeleteDir: return QStringLiteral("Delete folder");
    case ActionType::Skip: return QStringLiteral("Skip");
    }
    return {};
}

} // namespace obor
