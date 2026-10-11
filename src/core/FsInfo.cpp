#include "FsInfo.h"

#include <QDir>
#include <QFileInfo>
#include <QStorageInfo>

namespace obor::FsInfo {

namespace {

QString nearestExistingDir(const QString &path)
{
    QFileInfo info(QDir::cleanPath(QFileInfo(path).absoluteFilePath()));
    while (!info.exists()) {
        const QString parent = info.absolutePath();
        if (parent == info.absoluteFilePath())
            return {};
        info = QFileInfo(parent);
    }
    return info.absoluteFilePath();
}

} // namespace

VolumeInfo query(const QString &path)
{
    VolumeInfo info;
    if (path.trimmed().isEmpty())
        return info;
    const QString existing = nearestExistingDir(path);
    if (existing.isEmpty())
        return info;
    const QStorageInfo storage(existing);
    if (!storage.isValid())
        return info;
    info.valid = true;
    info.fileSystem = QString::fromUtf8(storage.fileSystemType());
    info.displayName = storage.displayName();
    info.rootPath = storage.rootPath();
    info.bytesFree = storage.bytesAvailable();
    info.bytesTotal = storage.bytesTotal();
    info.fatLike = isFatLike(info.fileSystem);
    return info;
}

bool isFatLike(const QString &fileSystem)
{
    static const QStringList fat = {QStringLiteral("fat"),   QStringLiteral("fat12"),
                                    QStringLiteral("fat16"), QStringLiteral("fat32"),
                                    QStringLiteral("vfat"),  QStringLiteral("exfat"),
                                    QStringLiteral("msdos")};
    return fat.contains(fileSystem.toLower());
}

int autoToleranceMs(const VolumeInfo &source, const VolumeInfo &destination)
{
    return (source.fatLike || destination.fatLike) ? 2000 : 0;
}

bool caseInsensitive(const VolumeInfo &source, const VolumeInfo &destination)
{
#ifdef Q_OS_WIN
    Q_UNUSED(source)
    Q_UNUSED(destination)
    return true;
#else
    return source.fatLike || destination.fatLike;
#endif
}

} // namespace obor::FsInfo
