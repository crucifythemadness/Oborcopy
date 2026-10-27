#include "VolumeCheck.h"

#include <QDir>
#include <QFileInfo>

namespace obor::VolumeCheck {

namespace {

#ifdef Q_OS_WIN
constexpr auto kCase = Qt::CaseInsensitive;
#else
constexpr auto kCase = Qt::CaseSensitive;
#endif

QString withSlash(const QString &path)
{
    QString result = QDir::fromNativeSeparators(path);
    if (!result.endsWith(QLatin1Char('/')))
        result += QLatin1Char('/');
    return result;
}

bool isUnder(const QString &path, const QString &root)
{
    return withSlash(path).startsWith(withSlash(root), kCase);
}

bool sameRoot(const QString &a, const QString &b)
{
    return withSlash(a).compare(withSlash(b), kCase) == 0;
}

// "E:/..." -> 'E'; anything else (UNC, Unix) -> null.
QChar driveLetter(const QString &path)
{
    if (path.size() >= 3 && path.at(0).isLetter() && path.at(1) == QLatin1Char(':')
        && path.at(2) == QLatin1Char('/'))
        return path.at(0).toUpper();
    return {};
}

} // namespace

Result resolve(const QString &path, const VolumeRef &ref, const VolumeInfo &atPath,
               const QList<VolumeInfo> &named)
{
    Result result;
    result.path = path;
    if (ref.isEmpty() || ref.root.isEmpty() || !isUnder(path, ref.root))
        return result;
    if (atPath.valid && atPath.label == ref.label) {
        result.status = Status::Ok;
        result.volume = atPath;
        return result;
    }
    if (named.size() == 1) {
        const QString relative = withSlash(path).mid(withSlash(ref.root).size());
        result.status = Status::Moved;
        result.path = QDir::cleanPath(withSlash(named.first().rootPath) + relative);
        result.volume = named.first();
        return result;
    }
    if (named.size() > 1) {
        result.status = Status::Ambiguous;
        return result;
    }
    if (atPath.valid && sameRoot(atPath.rootPath, ref.root)) {
        result.status = Status::WrongDisk;
        result.volume = atPath;
        return result;
    }
    result.status = Status::Missing;
    return result;
}

Result resolve(const QString &path, const VolumeRef &ref)
{
    if (ref.isEmpty())
        return resolve(path, ref, VolumeInfo(), {});
    return resolve(path, ref, FsInfo::query(path), FsInfo::volumesNamed(ref.label));
}

VolumeRef learn(const VolumeRef &ref, const VolumeInfo &volume)
{
    if (!volume.valid || volume.label.isEmpty())
        return ref;
    if (!ref.isEmpty() && ref.label != volume.label)
        return ref;
    return {volume.label, volume.rootPath};
}

QString findElsewhere(const QString &path, const QList<VolumeInfo> &volumes,
                      const std::function<bool(const QString &)> &isDir, VolumeInfo *found)
{
    const QString clean = QDir::fromNativeSeparators(path);
    const QChar letter = driveLetter(clean);
    const QString rest = clean.mid(3);
    if (letter.isNull() || rest.isEmpty())
        return {};
    QString match;
    for (const VolumeInfo &volume : volumes) {
        const QString root = withSlash(volume.rootPath);
        const QChar other = driveLetter(root);
        if (!volume.valid || other.isNull() || root.size() != 3 || other == letter)
            continue;
        const QString candidate = root + rest;
        if (!isDir(candidate))
            continue;
        if (!match.isEmpty()) {
            if (found)
                *found = VolumeInfo();
            return {}; // on several drives: no way to tell which one is meant
        }
        match = candidate;
        if (found)
            *found = volume;
    }
    return match;
}

QString findElsewhere(const QString &path, VolumeInfo *found)
{
    return findElsewhere(path, FsInfo::mountedVolumes(),
                         [](const QString &dir) { return QFileInfo(dir).isDir(); }, found);
}

} // namespace obor::VolumeCheck
